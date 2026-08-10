#include "tooltip.h"

#include <qscrollbar>
#include <ranges>
#include <nlohmann/json.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>

#include "cccedict.h"
#include "screenshot.h"
#include "util.h"
#include "util_ocr.h"
#include "util_qt.h"
#include "util_utf8.h"

namespace iwra {
	TooltipWindow::TooltipWindow(
		QWidget*                       parent,
		const std::shared_ptr<Config>& config
	):
		QMainWindow{parent},
		hover_hotkey{new QHotkey(QKeySequence("ctrl+shift+3"), true, this)},
		dictionary_parser{std::make_shared<CCCEdictDictParser>()},
		central_widget{new QWidget(this)},
		layout{new QVBoxLayout(central_widget)},
		scrollbar{new QScrollArea(this)},
		anki_interface{std::make_shared<AnkiInterface>(config)},
		config{config} {
		// function definition start

		if (!config) {
			spdlog::error("TooltipWindow cannot be created because config is null");
			throw std::runtime_error("TooltipWindow cannot be created because config is null");
		}

		if (config->getAnkiNoteType().has_value()) {
			anki_interface->fillConfigNoteFields();
		} else {
			spdlog::warn(
				"No Anki note type supplied, to allow anki support, please fill anki: note-type field in the config"
				" and restart the application to automatically generate the fields"
			);
		}

		if (const std::optional dict_path_opt = config->getDictPath();
			dict_path_opt.has_value()) {
			dictionary_parser->load(dict_path_opt.value());
		} else {
			spdlog::error("TooltipWindow has no dictionary file");
		}

		connect(
			hover_hotkey,
			&QHotkey::activated,
			this,
			[this]() {
				if (timer_id != 0) {
					spdlog::warn("[Tooltip] timer is already running");
					return;
				}
				if (is_hovering) {
					show();
				} else {
					hide();
				}
				refreshHovering();
				timer_id = startTimer(0, Qt::PreciseTimer);
			}
		);

		connect(
			hover_hotkey,
			&QHotkey::released,
			this,
			[this]() {
				if (timer_id == 0) {
					spdlog::warn("[Tooltip] can't kill timer that hasn't started");
					return;
				}
				killTimer(timer_id);
				timer_id = 0;
			}
		);

		setWindowFlags(
			Qt::FramelessWindowHint |
			Qt::Tool |
			Qt::NoDropShadowWindowHint |
			Qt::WindowStaysOnTopHint
		);

		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(8);
		layout->setAlignment(Qt::AlignTop);

		setCentralWidget(scrollbar);
		compactifyWidget(central_widget);
		central_widget->setLayout(layout);

		scrollbar->setWidget(central_widget);
		scrollbar->setWidgetResizable(true);
		compactifyWidget(scrollbar);
		scrollbar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollbar->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scrollbar->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

		setFixedSize(config->getTooltipWidth(), config->getTooltipHeight());
	}

	bool TooltipWindow::initDictEntry(const std::string& key, const std::string& phrase) {
		auto       it        = phrase.begin();
		const auto end       = phrase.end();
		dictionary_data[key] = {};

		std::string lookup_string;
		do {
			lookup_string += toUtf8(utf8::next(it, end));
			if (auto dict_entry = dictionary_parser->getEntry(lookup_string);
				!dict_entry.empty()) {
				dictionary_data[key].entries.append_range(dict_entry);
				dictionary_data[key].phrase = lookup_string;
			}
		} while (it != end);
		std::ranges::reverse(dictionary_data[key].entries);
		return !dictionary_data[key].entries.empty();
	}

	// ReSharper disable once CppInconsistentNaming
	std::vector<OCRBlock> TooltipWindow::processOCRResults(
		const std::vector<OCRResult>& unprocessed_results,
		const cv::Point&              topleft
	) {
		std::vector<OCRBlock> res;
		res.reserve(unprocessed_results.size());
		for (const auto& [text_rect, text] : unprocessed_results) {
			auto         rect       = text_rect.rect;
			const Poly2I moved_rect = {
				rect[0] + topleft,
				rect[1] + topleft,
				rect[2] + topleft,
				rect[3] + topleft,
			};
			const std::array xs            = {rect[0].x, rect[1].x, rect[2].x, rect[3].x};
			const std::array ys            = {rect[0].y, rect[1].y, rect[2].y, rect[3].y};
			const auto       [left, right] = std::minmax_element(xs.begin(), xs.end());
			const auto       [top, bottom] = std::minmax_element(ys.begin(), ys.end());
			bool             horizontal    = (*right - *left) > (*bottom - *top);

			auto split_line = ocrSplitText(moved_rect, text, horizontal);

			std::vector<int> intersections = std::views::iota(std::size_t{0}, res.size())
			                                 | std::views::filter(
				                                 [res, &moved_rect](const std::size_t i) -> int {
					                                 return res[i].horizontal && intersects(res[i].poly, moved_rect);
				                                 }
			                                 )
			                                 | std::ranges::to<std::vector<int> >();
			if (intersections.empty()) {
				std::vector<cv::Point> poly{};
				poly.append_range(moved_rect);
				res.emplace_back(split_line, poly, horizontal);
				continue;
			}

			if (horizontal) {
				std::ranges::reverse(intersections);
			}

			OCRBlock& first = res[intersections[0]];
			first.results.append_range(split_line);
			first.poly = union_(first.poly, moved_rect);
			for (int i = 1; i < intersections.size(); ++i) {
				OCRBlock curr = res[intersections[i]];
				first.results.append_range(curr.results);
				first.poly = union_(first.poly, curr.poly);
				res.erase(res.begin() + intersections[i]);
			}
		}
		return res;
	}

	std::vector<OCRResultPacked> TooltipWindow::ocrSplitText(
		const Poly2I& rect,
		const Text&   text,
		const bool    horizontal
	) {
		auto                         it  = text.text.begin();
		const auto                   end = text.text.end();
		std::vector<OCRResultPacked> out{};
		out.reserve(text.char_lengths.size());
		for (const auto& [lower, upper] : text.char_lengths) {
			std::string utf16_it;
			try {
				utf8::append(utf8::next(it, end), utf16_it);
			} catch (const utf8::not_enough_room& _) {
				continue;
			}
			Poly2I char_rect;
			if (horizontal) {
				char_rect = {
					cv::Point{static_cast<int>(std::lerp(rect[0].x, rect[1].x, lower)), rect[0].y},
					cv::Point{static_cast<int>(std::lerp(rect[0].x, rect[1].x, upper)), rect[1].y},
					cv::Point{static_cast<int>(std::lerp(rect[3].x, rect[2].x, upper)), rect[2].y},
					cv::Point{static_cast<int>(std::lerp(rect[3].x, rect[2].x, lower)), rect[3].y}
				};
			} else {
				char_rect = {
					cv::Point{rect[0].x, static_cast<int>(std::lerp(rect[0].y, rect[3].y, lower))},
					cv::Point{rect[1].x, static_cast<int>(std::lerp(rect[1].y, rect[2].y, lower))},
					cv::Point{rect[2].x, static_cast<int>(std::lerp(rect[1].y, rect[2].y, upper))},
					cv::Point{rect[3].x, static_cast<int>(std::lerp(rect[0].y, rect[3].y, upper))}
				};
			}
			out.emplace_back(char_rect, utf16_it);
		}

		return std::move(out);
	}

	void TooltipWindow::updateWindowPosition() {
		if (!current_word) {
			return;
		}

		const auto [screen_width, screen_height] = getScreenSize();

		int top;
		// choose above or below hover word based on which haas more room
		if (const int room_left_top = getTop(current_word->rect);
			screen_height - getBottom(current_word->rect) > room_left_top) {
			// window is too tall (it goes above top of screen)
			top = getBottom(current_word->rect) + height();
		} else {
			// window can extend up and is below screen
			top = room_left_top;
		}
		int left;
		if (getRight(current_word->rect) + width() > screen_width) {
			// window is too width (it goes past right of screen)
			left = screen_width - width();
		} else {
			// window can extend right and is left of screen edge
			left = getLeft(current_word->rect);
		}
		move(left, top - height());
	}

	const DictionaryData* TooltipWindow::getDictDataOrInit(const std::string& key, const std::string& phrase) {
		const auto it = dictionary_data.find(key);
		if (it == dictionary_data.end()) {
			if (!initDictEntry(key, phrase)) {
				return nullptr;
			}
			return &dictionary_data.at(key);
		}

		if (it->second.entries.empty()) {
			if (!initDictEntry(key, phrase)) {
				return nullptr;
			}
			return &dictionary_data.at(key);
		}
		return &it->second;
	}

	void TooltipWindow::updateWindowEntry(
		const DictionaryData* dict_data,
		const std::string&    phrase,
		const std::string&    sentence,
		const long long       offset
	) {
		std::size_t i = 0;
		for (; i < dict_data->entries.size(); ++i) {
			if (i < entries.size()) {
				entries[i]->show();
				entries[i]->update(dict_data->entries[i], phrase, sentence, offset);
			} else {
				auto* new_entry = new TooltipEntry(this, anki_interface, config);
				new_entry->setFixedWidth(256 - 12);
				layout->addWidget(new_entry);
				entries.push_back(new_entry);
				entries[i]->update(dict_data->entries[i], phrase, sentence, offset);
			}
		}
		for (; i < entries.size(); ++i) {
			entries[i]->hide();
		}

		scrollbar->verticalScrollBar()->setSliderPosition(0);

		updateWindowPosition();
	}

	std::vector<std::string> splitHanzi(const std::string& hanzi, const std::string& pinyin) {
		std::string              post_pinyin = pinyin + ' ';
		std::vector<std::size_t> lengths;
		std::size_t              prev = 0;
		for (std::size_t it = post_pinyin.find(' ');
		     it != std::string::npos;
		     prev = it,
		     it   = post_pinyin.find(' ', it + 1)) {
			if (const int number = post_pinyin[it - 1] - '0';
				1 >= number || number <= 5) {
				lengths.push_back(1);
				continue;
			}

			post_pinyin.erase(it, 1);
			it -= 1;
			if (it > 0) {
				if (const int prev_number = post_pinyin[it - 1] - '0';
					1 < prev_number && prev_number > 5) {
					lengths.back() += it - prev;
					continue;
				}
			}
			lengths.push_back(it - prev);
		}

		std::vector<std::string> res;

		std::size_t          total_len = 0;
		const std::u16string hanzi_u16 = utf8::utf8to16(hanzi);
		for (const auto length : lengths) {
			res.push_back(utf8::utf16to8(hanzi_u16.substr(total_len, length)));
			total_len += length;
		}
		return res;
	}

	// 40ms
	void TooltipWindow::updateResRect(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect) {
		results       = processOCRResults(new_res, new_rect.tl());
		rect          = new_rect;
		current_block = nullptr;
		current_word  = nullptr;
	}

	void TooltipWindow::refreshWindow() {
		if (!current_word) {
			return;
		}

		if (current_phrase == current_word->text) {
			return;
		}

		const std::string phrase    = getPhrase(current_word, current_block);
		const auto*       dict_data = getDictDataOrInit(current_word->text, phrase);
		if (!dict_data) {
			return;
		}

		current_phrase             = current_word->text;
		const std::string sentence = getSentence(current_block);
		const long long   offset   = std::distance(current_block->results.begin()._Ptr, current_word);

		updateWindowEntry(dict_data, phrase, sentence, offset);
	}

	void TooltipWindow::refreshHovering() {
		if (results.empty()) {
			is_hovering = false;
			return;
		}

		const QPoint qt_cursor_pos = QCursor::pos();

		const cv::Point mouse_pos{qt_cursor_pos.x(), qt_cursor_pos.y()};

		// if mouse is in the captured rect
		if (!rect.contains(mouse_pos)) {
			is_hovering = false;
			return;
		}

		// caching previous rect (optimisation)
		if (is_hovering && current_word && !current_word->rect.empty() && pointPolygonTest(
			    current_word->rect,
			    mouse_pos,
			    false
		    ) > 0) {
			return;
		}

		auto intersect_iter = results.end();
		if (!results.empty()) {
			for (auto block = results.begin(); block != intersect_iter; ++block) {
				if (block->poly.size() <= 1) {
					continue;
				}
				if (pointPolygonTest(block->poly, mouse_pos, false) >= 0) {
					intersect_iter = block;
					break;
				}
			}
		}

		// mouse isn't in any of the OCR areas
		if (intersect_iter == results.end()) {
			is_hovering = false;
			return;
		}

		const auto word_iter = std::ranges::find_if(
			intersect_iter->results,
			[&mouse_pos](const OCRResultPacked& res) -> bool {
				// returns positive (inside), negative (outside), or zero (on an edge) value
				return pointPolygonTest(res.rect, mouse_pos, false) > 0;
			}
		);

		// mouse isn't in any of the word areas
		if (word_iter == intersect_iter->results.end()) {
			is_hovering = false;
			return;
		}

		is_hovering   = true;
		current_word  = word_iter._Ptr;
		current_block = intersect_iter._Ptr;
		refreshWindow();
	}

	std::string TooltipWindow::getSentence(OCRBlock* hover_block) {
		return hover_block->results
		       | std::ranges::views::transform(
			       [](auto& r) -> std::string& {
				       return r.text;
			       }
		       )
		       | std::views::join | std::ranges::to<std::string>();
	}

	std::string TooltipWindow::getPhrase(const OCRResultPacked* hover_word, const OCRBlock* hover_block) {
		std::string result;
		for (const auto* curr = hover_word; curr != hover_block->results.end()._Ptr; ++curr) {
			result += curr->text;
		}
		return result;
	}

	void TooltipWindow::timerEvent(QTimerEvent* event) {
		if (is_hovering) {
			show();
		} else {
			hide();
		}
		refreshHovering();
		QMainWindow::timerEvent(event);
	}
}
