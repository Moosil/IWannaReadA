#include "tooltip.h"


#include <clip.h>
#include <ranges>
#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>

#include "dict_parser.h"
#include "screenshot.h"
#include "util.h"
#include "util_ocr.h"
#include "util_qt.h"
#include "util_utf8.h"


namespace iwra {
	TooltipWnd::TooltipWnd(
		QWidget*                                parent,
		const std::shared_ptr<DictParser>&      parser,
		const std::shared_ptr<Anki::Interface>& anki
	):
		QMainWindow{parent},
		hover_hotkey{new QHotkey(QKeySequence("ctrl+shift+3"), true, this)},
		parser{parser},
		centralWidget{new QWidget(this)},
		layout{new QVBoxLayout(centralWidget)},
		scrollbar{new QScrollArea(this)},
		anki{anki} {

		connect(hover_hotkey, &QHotkey::activated, this, [this]() {
			if (timer_id != 0) {
				spdlog::warn("timer is already running");
				return;
			}
			timer_id = startTimer(0, Qt::PreciseTimer);
		});

		connect(hover_hotkey, &QHotkey::released, this, [this]() {
			if (timer_id == 0) {
				spdlog::warn("can't kill timer that hasn't started");
				return;
			}
			killTimer(timer_id);
			timer_id = 0;
		});

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
		compactifyWidget(centralWidget);
		centralWidget->setLayout(layout);

		scrollbar->setWidget(centralWidget);
		scrollbar->setWidgetResizable(true);
		compactifyWidget(scrollbar);
		scrollbar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollbar->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scrollbar->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

		setMinimumSize(256, 128);
		setMaximumSize(256, 256);
	}

	bool TooltipWnd::initDictEntry(const std::string& key, const std::string& phrase) {
		auto       it        = phrase.begin();
		const auto end       = phrase.end();
		dictionary_data[key] = {};

		std::string lookup_string;
		do {
			lookup_string += toUtf8(utf8::next(it, end));
			if (auto dict_entry = parser->get_entry(lookup_string);
				!dict_entry.empty()) {
				dictionary_data[key].entries.append_range(dict_entry);
				dictionary_data[key].phrase = lookup_string;
			}
		} while (it != end);
		std::ranges::reverse(dictionary_data[key].entries);
		return !dictionary_data[key].entries.empty();
	}

	std::vector<OCRResultPacked> TooltipWnd::ocrSplitText(const Poly2I& rect, const Text& text, const bool horizontal) {
		auto                         it  = text.text.begin();
		const auto                   end = text.text.end();
		std::vector<OCRResultPacked> out{};
		out.reserve(text.char_lengths.size());
		for (const auto& [lower, upper] : text.char_lengths) {
			std::string utf16i;
			try {
				utf8::append(utf8::next(it, end), utf16i);
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
			out.emplace_back(char_rect, utf16i);
		}

		return std::move(out);
	}

	void TooltipWnd::processOCRResults(
		const std::vector<OCRResult>& res,
		const cv::Point&              topleft,
		std::vector<OCRBlock>&        out
	) {
		out.clear();
		out.reserve(res.size());
		for (const auto& [rect, text] : res) {
			const Poly2I moved_rect = {
				rect.rect[0] + topleft,
				rect.rect[1] + topleft,
				rect.rect[2] + topleft,
				rect.rect[3] + topleft,
			};
			const std::array xs            = {rect.rect[0].x, rect.rect[1].x, rect.rect[2].x, rect.rect[3].x};
			const std::array ys            = {rect.rect[0].y, rect.rect[1].y, rect.rect[2].y, rect.rect[3].y};
			const auto       [left, right] = std::minmax_element(xs.begin(), xs.end());
			const auto       [top, bottom] = std::minmax_element(ys.begin(), ys.end());
			bool             horizontal    = (*right - *left) > (*bottom - *top);

			auto split_line = ocrSplitText(moved_rect, text, horizontal);

			std::vector<int> intersections = std::views::iota(std::size_t{0}, out.size())
			                                 | std::views::filter(
				                                 [out, &moved_rect](const std::size_t i) -> int {
					                                 return out[i].horizontal && intersects(out[i].poly, moved_rect);
				                                 }
			                                 )
			                                 | std::ranges::to<std::vector<int> >();
			if (intersections.empty()) {
				std::vector<cv::Point> poly{};
				poly.append_range(moved_rect);
				out.emplace_back(split_line, poly, horizontal);
			} else {
				if (horizontal) {
					std::ranges::reverse(intersections);
					OCRBlock& first = out[intersections[0]];
					first.results.append_range(split_line);
					first.poly = union_(first.poly, moved_rect);
					for (int i = 1; i < intersections.size(); i++) {
						OCRBlock curr = out[intersections[i]];
						first.results.append_range(curr.results);
						first.poly = union_(first.poly, curr.poly);
						out.erase(out.begin() + intersections[i]);
					}
				} else {
					OCRBlock& first = out[intersections[0]];
					first.results.append_range(split_line);
					first.poly = union_(first.poly, moved_rect);
					for (int i = 1; i < intersections.size(); i++) {
						OCRBlock curr = out[intersections[i]];
						first.results.append_range(curr.results);
						first.poly = union_(first.poly, curr.poly);
						out.erase(out.begin() + intersections[i]);
					}
				}
			}
		}
	}

	void TooltipWnd::updateWindowPosition() {
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

	const DictionaryData* TooltipWnd::getDictDataOrInit(const std::string& key, const std::string& phrase) {
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

	void TooltipWnd::updateWindowEntry(
		const DictionaryData* dict_data,
		const std::string&    phrase,
		const std::string&    sentence
	) {
		std::size_t i = 0;
		for (; i < dict_data->entries.size(); ++i) {
			if (i < entries.size()) {
				entries[i]->show();
				entries[i]->update(dict_data->entries[i], phrase, sentence);
			} else {
				auto* new_entry = new TooltipEntry(this, anki);
				new_entry->setFixedWidth(256 - 12);
				layout->addWidget(new_entry);
				entries.push_back(new_entry);
				entries[i]->update(dict_data->entries[i], phrase, sentence);
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
		for (std::size_t it = post_pinyin.find(' '); it != std::string::npos; prev = it, it = post_pinyin.find(
			                                                                      ' ',
			                                                                      it + 1
		                                                                      )) {
			if (const int number = post_pinyin[it - 1] - '0';
				1 < number && number > 5) {
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
			} else {
				lengths.push_back(1);
			}
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

	void TooltipWnd::refreshWindow() {
		if (!current_word) {
			return;
		}

		if (current_phrase == current_word->text) {
			return;
		}

		const std::string phrase = getPhrase(current_word, current_block);
		spdlog::info("phrase: {}", phrase);
		const auto* dict_data = getDictDataOrInit(current_word->text, phrase);
		if (!dict_data) {
			return;
		}

		current_phrase             = current_word->text;
		const std::string sentence = getSentence(current_block);

		updateWindowEntry(dict_data, phrase, sentence);
	}

	void TooltipWnd::addAnkiCard(
		const std::string& character,
		const std::string& phrase,
		const std::string& pinyin,
		const std::string& sentence,
		const std::string& definition
	) const {
		auto              [find_pos_first, find_pos_second] = utf8Find(sentence, character);
		const std::string sentence_add                      = std::format(
			"{}{{{{c1::{}}}}}{}",
			std::string(sentence.begin(), find_pos_first),
			phrase,
			std::string(find_pos_second, sentence.end())
		);

		anki->add_note(phrase, pinyin, definition, sentence_add);
	}

	std::string TooltipWnd::getSentence(OCRBlock* hover_block) {
		return hover_block->results
		       | std::ranges::views::transform(
			       [](auto& r) -> std::string& { return r.text; }
		       )
		       | std::views::join | std::ranges::to<std::string>();
	}

	std::string TooltipWnd::getPhrase(const OCRResultPacked* hover_word, const OCRBlock* hover_block) {
		std::string result;
		for (const auto* curr = hover_word; curr != hover_block->results.end()._Ptr; ++curr) {
			result += curr->text;
		}
		return result;
	}

	void TooltipWnd::timerEvent(QTimerEvent* event) {
		if (is_hovering) {
			show();
		} else {
			hide();
		}
		refreshHovering();
		QMainWindow::timerEvent(event);
	}

	// 40ms
	void TooltipWnd::updateResRect(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect) {
		processOCRResults(new_res, {new_rect.x, new_rect.y}, results);
		rect          = new_rect;
		current_block = nullptr;
		current_word  = nullptr;
	}

	void TooltipWnd::refreshHovering() {
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
		if (is_hovering) {
			if (current_word && !current_word->rect.empty() && cv::pointPolygonTest(
				    current_word->rect,
				    mouse_pos,
				    false
			    ) > 0) {
				return;
			}
		}

		const auto intersect_iter = std::ranges::find_if(
			results,
			[&mouse_pos](const OCRBlock& block) -> bool {
				// returns positive (inside), negative (outside), or zero (on an edge) value
				if (block.poly.empty())
					return false;
				return cv::pointPolygonTest(block.poly, mouse_pos, false) > 0;
			}
		);

		// mouse isn't in any of the OCR areas
		if (intersect_iter == results.end()) {
			is_hovering = false;
			return;
		}

		const auto word_iter = std::ranges::find_if(
			intersect_iter->results,
			[&mouse_pos](const OCRResultPacked& res) -> bool {
				// returns positive (inside), negative (outside), or zero (on an edge) value
				return cv::pointPolygonTest(res.rect, mouse_pos, false) > 0;
			}
		);

		if (word_iter == intersect_iter->results.end()) {
			is_hovering = false;
			return;
		}

		is_hovering   = true;
		current_word  = word_iter._Ptr;
		current_block = intersect_iter._Ptr;
		refreshWindow();
	}
}
