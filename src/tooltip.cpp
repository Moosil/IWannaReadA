#include "tooltip.h"


#include <clip.h>
#include <filesystem>
#include <ranges>
#include <WebView2.h>
#include <Windowsx.h>
#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>
#include <wrl/event.h>

#include "dict_parser.h"
#include "implwebview2.h"
#include "log.h"
#include "util.h"
#include "util_ocr.h"
#include "util_text.h"
#include "util_utf8.h"


namespace iwra {
	TooltipWnd::TooltipWnd(
		const std::vector<OCRResult>& res,
		const cv::Rect& rect,
		const std::filesystem::path& webpage_path,
		const std::shared_ptr<DictParser>& parser,
		const std::shared_ptr<Anki::Interface>& anki
	): rect{rect}, parser{parser}, anki{anki} {
		processOCRResults(res, cv::Point{rect.x, rect.y}, results);

		constexpr int style           = WS_POPUP;
		constexpr int extended_styles = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
		width                    = min_width;
		height                   = min_height;

		hwnd = CreateWindowEx(
			extended_styles,
			className.c_str(),
			"tt",
			style,
			0,
			0,
			width,
			height,
			nullptr,
			nullptr,
			GetModuleHandle(nullptr),
			this
		);

		webpage_html = readFile(webpage_path);

		log(SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE), "SetWindowDisplayAffinity");
	}

	void TooltipWnd::initWebView2() {
		HRESULT err = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		log(err, "CoInitializeEx", ERR_LEVEL::FATAL);
		wchar_t* version;
		err = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
		log(err, "GetAvailableCoreWebView2BrowserVersionString", ERR_LEVEL::WARN);
		if (SUCCEEDED(err)) {
			CoTaskMemFree(version);
		}
		wv_init = std::make_unique<WebView2::Impl>(
			hwnd,
			RECT{0, 0, width, height},
			[this](ICoreWebView2Controller* controller, ICoreWebView2* wv) {
				if (!controller || !wv) {
					return;
				}
				wv_controller = controller;
				webview       = wv;

				const HRESULT nav_subscribe_err = webview->add_NavigationCompleted(
					Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
						[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
							inited_web_view2 = true;
							onNavigationComplete();
							return S_OK;
						}
					).Get(),
					nullptr
				);
				log(nav_subscribe_err, "ICoreWebView2::add_NavigationCompleted", ERR_LEVEL::WARN);

				const HRESULT nav_err = webview->NavigateToString(
					utf8ToWide(webpage_html).c_str()
				);
				log(nav_err, "ICoreWebView2::NavigateToString", ERR_LEVEL::FATAL);

				if (!is_hovering) {
					ShowWindow(hwnd, SW_HIDE);
				}
			}
		);
		wv_init->try_init_env();
	}

	void TooltipWnd::initCurrDict() {
		if (!hover_word || !hover_block || results.empty())
			return;

		spdlog::info("inited dict of {}", hover_word->text);
		std::string       lookup_string;
		const std::string first_char = hover_word->text;
		dictionary_data[first_char]  = {};
		for (const auto* curr = hover_word; curr != hover_block->results.data() + hover_block->results.size(); ++curr) {
			lookup_string += curr->text;
			if (auto dict_entry = parser->get_entry(lookup_string);
				!dict_entry.empty()) {
				spdlog::info("loading entry: {}", lookup_string);
				dictionary_data[first_char].entries.append_range(dict_entry);
				dictionary_data[first_char].phrase = lookup_string;
			}
		};
		std::ranges::reverse(dictionary_data[first_char].entries);
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
				out.emplace_back(split_line, horizontal, poly);
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

	void TooltipWnd::updateWindowSize() const {
		SetWindowPos(
			hwnd,
			HWND_TOPMOST,
			0,
			0,
			width,
			height,
			SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW
		);
		InvalidateRect(hwnd, nullptr, FALSE);
	}

	void TooltipWnd::updateWindowPosition() const {
		if (!hover_word)
			return;

		const auto [screen_width, screen_height] = getScreenSize();

		int top;
		// choose above or below hover word based on which haas more room
		if (const int room_left_top = getTop(hover_word->rect);
			screen_height - getBottom(hover_word->rect) > room_left_top) {
			// window is too tall (it goes above top of screen)
			top = getBottom(hover_word->rect) + height;
		} else {
			// window can extend up and is below screen
			top = room_left_top;
		}
		int left;
		if (getRight(hover_word->rect) + width > screen_width) {
			// window is too width (it goes past right of screen)
			left = screen_width - width;
		} else {
			// window can extend right and is left of screen edge
			left = getLeft(hover_word->rect);
		}
		SetWindowPos(hwnd, HWND_TOPMOST, left, top - height, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	std::vector<std::string> splitHanzi(const std::string& hanzi, const std::string& pinyin) {
		std::string post_pinyin = pinyin+' ';
		std::vector<std::size_t> lengths;
		std::size_t prev = 0;
		for (std::size_t it = post_pinyin.find(' '); it != std::string::npos; prev = it, it = post_pinyin.find(' ', it+1)) {
			if (const int number = post_pinyin[it-1] - '0';
				1 < number && number > 5) {
				post_pinyin.erase(it, 1);
				it -= 1;
				if (it > 0) {
					if (const int prev_number = post_pinyin[it-1] - '0';
						1 < prev_number && prev_number > 5) {
						lengths.back() += it-prev;
						continue;
					}
				}
				lengths.push_back(it-prev);
			} else {
				lengths.push_back(1);
			}
		}

		std::vector<std::string> res;

		std::size_t total_len = 0;
		const std::u16string hanzi_u16 = utf8::utf8to16(hanzi);
		for (const auto length : lengths) {
			res.push_back(utf8::utf16to8(hanzi_u16.substr(total_len, length)));
			total_len += length;
		}
		return res;
	}

	void TooltipWnd::refreshWindow() {
		need_refresh = false;
		if (!is_hovering)
			return;

		if (!inited_web_view2)
			return;

		if (!hover_word)
			return;

		const auto it = dictionary_data.find(hover_word->text);
		if (it == dictionary_data.end()) {
			initCurrDict();
			need_refresh = true;
			return;
		}
		// if (it != dictionary_data.end()) {
		// 	spdlog::info(it->first);
		// }

		if (it->second.entries.empty()) {
			//TODO deal with dictionary having that entry but nothing in that entry
			spdlog::warn("{}'s dictionary had entry but no contents: NOT IMPLEMENTED", hover_word->text);
		}

		spdlog::info("loading {}", getPhrase(hover_word, hover_block));
		auto&          dict_data = it->second;
		nlohmann::json page_data = nlohmann::json::array();
		for (const auto& entry : dict_data.entries) {
			spdlog::info("adding {} to tooltip", entry.get_simp());
			// if entry.simp is a prefix of the hovered phrase
			if (const std::string phrase = getPhrase(hover_word, hover_block);
				!phrase.starts_with(entry.get_simp()) && !phrase.starts_with(entry.get_trad())) {
				spdlog::info("nvm");
				continue;
			}

			nlohmann::json def_json = nlohmann::json::array();
			for (const auto& def : entry.definitions) {
				def_json.push_back(def);
			}

			nlohmann::json words = nlohmann::json::array();
			for (const auto& [characters] : entry.word) {
				nlohmann::json word_json = nlohmann::json::array();
				for (const auto& [simp, trad, pinyin] : characters) {
					nlohmann::json char_json = nlohmann::json::object();
					char_json["simp"]        = simp;
					char_json["trad"]        = trad;
					char_json["pinyin"]      = pinyin;
					word_json.push_back(char_json);
				}
				words.push_back(word_json);
			}

			nlohmann::json entry_json = nlohmann::json::object();
			entry_json["words"] = words;
			entry_json["def"]         = def_json;
			entry_json["c_word"]      = dict_data.phrase;
			entry_json["c_sent"]      = getSentence(hover_block);
			page_data.push_back(entry_json);
		}
		const std::string page_data_str = page_data.dump();
		const std::string script        = "setPage(" + page_data_str + ")";
		const auto        script_utf16  = utf8::utf8to16(script);

		width = std::max(min_width, max_webpage_width);
		spdlog::info("script: {}", script);

		if (dict_data.height == -1) {
			height = min_height;
			updateWindowSize();
			updateWindowPosition();
			const Poly2I  hover_word_rect = hover_word->rect;
			const HRESULT err0            = webview->ExecuteScript(
				utf8ToWide(script).c_str(),
				Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					// ReSharper disable once CppParameterMayBeConst
					[this, &dict_data, &hover_word_rect](HRESULT errorCode0, LPCWSTR resultObjectAsJson) -> HRESULT {
						log(errorCode0, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
						const HRESULT err1 = webview->ExecuteScript(
							get_height_script,
							Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
								// ReSharper disable CppParameterMayBeConst
								[this, &dict_data, &hover_word_rect](HRESULT errorCode1, LPCWSTR result) -> HRESULT {
									// ReSharper restore CppParameterMayBeConst
									log(errorCode1, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
									if (SUCCEEDED(errorCode1) && result && is_hovering) {
										const int calc_webpage_height = _wtoi(result);

										height = std::clamp(
											std::min(
												calc_webpage_height,
												std::max(
													getTop(hover_word_rect),
													getScreenSize().second - getBottom(hover_word_rect)
												)
											),
											min_height,
											max_height
										);
										dict_data.height = calc_webpage_height;
										updateWindowSize();
										updateWindowPosition();
									}
									return S_OK;
								}
							).Get()
						);
						log(err1, "ICoreWebView2::ExecuteScript", ERR_LEVEL::WARN);
						return S_OK;
					}
				).Get()
			);
			log(err0, "ICoreWebView2::ExecuteScript", ERR_LEVEL::FATAL);
		} else {
			const HRESULT err0 = webview->ExecuteScript(
				utf8ToWide(script).c_str(),
				Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					// ReSharper disable once CppParameterMayBeConst
					[](HRESULT errorCode0, LPCWSTR resultObjectAsJson) -> HRESULT {
						log(errorCode0, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
						return S_OK;
					}
				).Get()
			);
			log(err0, "ICoreWebView2::ExecuteScript", ERR_LEVEL::FATAL);

			height = std::clamp(
				std::min(
					dict_data.height,
					std::max(
						getTop(hover_word->rect),
						getScreenSize().second - getBottom(hover_word->rect)
					)
				),
				min_height,
				max_height
			);
			updateWindowSize();
			updateWindowPosition();
		}
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	HRESULT TooltipWnd::onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
		wchar_t*      json_string;
		const HRESULT err = args->get_WebMessageAsJson(&json_string);
		log(err, "ICoreWebView2WebMessageReceivedEventArgs::get_WebMessageAsJson", ERR_LEVEL::WARN);

		nlohmann::json json = nlohmann::json::parse(wideToUtf8(json_string));
		if (const auto it = json.find("key"); it != json.end()) {
			if (it.value() == "contextmenu") {
				createContextMenu(
					json.at("x"),
					json.at("y"),
					json.at("character"),
					json.at("word"),
					json.at("pinyin"),
					json.at("sentence"),
					json.at("definition")
				);
			} else {
				//mousedown
			}
		}
		return S_OK;
	}

	void TooltipWnd::onNavigationComplete() {
		EventRegistrationToken token       = {};
		const HRESULT          add_msg_err = webview->add_WebMessageReceived(
			Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
				this,
				&TooltipWnd::onWebMessageReceived
			).Get(),
			&token
		);
		log(add_msg_err, "ICoreWebView2::add_WebMessageReceived", ERR_LEVEL::WARN);
	}


	void TooltipWnd::createContextMenu(
		const int          x,
		const int          y,
		const std::string& character,
		const std::string& phrase,
		const std::string& pinyin,
		const std::string& sentence,
		const std::string& definition
	) const {
		const HMENU& menu = CreatePopupMenu();
		AppendMenu(menu, MF_STRING, 1, "Copy character");
		AppendMenu(menu, MF_STRING, 2, "Copy phrase");
		AppendMenu(menu, MF_STRING, 3, "Copy sentence");
		AppendMenu(menu, MF_STRING, 4, "Add to Anki");
		const int selected = TrackPopupMenu(
			menu,
			TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_NOANIMATION,
			x,
			y,
			0,
			hwnd,
			nullptr
		);
		switch (selected) {
			case 1: {
				clip::set_text(character);
				break;
			}
			case 2: {
				clip::set_text(phrase);
				break;
			}
			case 3: {
				clip::set_text(sentence);
				break;
			}
			case 4: {
				const auto        find_pos     = utf8Find(sentence, utf8::peek_next(character.begin(), character.end()));
				const std::string sentence_add = std::format(
					"{}{{{{c1::{}}}}}{}",
					std::string(sentence.begin(), find_pos),
					phrase,
					std::string(find_pos + 1, sentence.end())
				);

				anki->add_note(phrase, pinyin, definition, sentence_add);
			}
			default:
				break;
		}
		DestroyMenu(menu);
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
		for (const auto* curr = hover_word; curr != hover_block->results.data() + hover_block->results.size(); ++curr) {
			result += curr->text;
		}
		return result;
	}

	LRESULT TooltipWnd::wndProc(const UINT msg, const WPARAM wparam, const LPARAM lparam) {
		switch (msg) {
			case WM_CREATE: {
				if (!inited_web_view2) {
					ShowWindow(hwnd, SW_SHOWNOACTIVATE);
					initWebView2();
				}
				break;
			}
			// case WM_KEYDOWN: {
			// 	break;
			// }
			case WM_CONTEXTMENU: {
				const auto x = GET_X_LPARAM(lparam);
				const int  y = GET_Y_LPARAM(lparam);
				if (hover_word) {
					createContextMenu(x, y, hover_word->text, hover_word->text, ":(", getSentence(hover_block), ":(");
				}
				break;
			}
			case WM_SIZE: {
				width  = LOWORD(lparam);
				height = HIWORD(lparam);

				if (wv_controller) {
					RECT rc;
					GetClientRect(hwnd, &rc);
					const HRESULT err = wv_controller->put_Bounds(rc);
					log(err, "ICoreWebView2Controller.put_Bounds", ERR_LEVEL::WARN);
				};
				break;
			}
			case WM_PAINT: {
				if (is_hovering) {
					PAINTSTRUCT ps;
					BeginPaint(hwnd, &ps);

					EndPaint(hwnd, &ps);
					return 0;
				}
				break;
			}
			case WM_DESTROY: {
				break;
			}
			case WM_NCDESTROY: {
				SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
				break;
			}
			default: {
			};
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	// ReSharper disable CppParameterMayBeConst
	LRESULT CALLBACK TooltipWnd::wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		// ReSharper restore CppParameterMayBeConst
		TooltipWnd* self;

		if (msg == WM_NCCREATE) {
			const auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
			self          = static_cast<TooltipWnd*>(cs->lpCreateParams);
			self->hwnd    = hwnd;
			SetLastError(0);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
			if (GetLastError() != 0) {
				return false;
			}
		} else {
			self = reinterpret_cast<TooltipWnd*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		if (self) {
			return self->wndProc(msg, wparam, lparam);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	TooltipWnd::~TooltipWnd() {
	}

	std::unique_ptr<TooltipWnd> TooltipWnd::initTooltip(
		const std::vector<OCRResult>& res,
		const cv::Rect&               rect,
		const std::filesystem::path&  webpage_path,
		const std::shared_ptr<DictParser>& parser,
		const std::shared_ptr<Anki::Interface>& anki
	) {

		if (!isInitialised) {
			WNDCLASS wc{};
			wc.lpfnWndProc   = &wndProcSetup;
			wc.hInstance     = GetModuleHandle(nullptr);
			wc.lpszClassName = className.c_str();
			wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW));
			if (!RegisterClassA(&wc)) {
				spdlog::error("registering window class failed");
				return nullptr;
			}
			isInitialised = true;
		}

		auto wnd = std::unique_ptr<TooltipWnd>(new TooltipWnd(res, rect, webpage_path, parser, anki));

		return wnd;
	}

	// 40ms
	void TooltipWnd::updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect) {
		processOCRResults(new_res, cv::Point{rect.x, rect.y}, results);

		rect        = new_rect;
		hover_block = nullptr;
		hover_word  = nullptr;
		if (GetAsyncKeyState(VK_SHIFT) & (1 << 15)) {
			refreshHovering();
			if (is_hovering) {
				ShowWindow(hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(hwnd);
			} else {
				if (inited_web_view2) {
					ShowWindow(hwnd, SW_HIDE);
				}
				UpdateWindow(hwnd);
			}
		}
	}

	void TooltipWnd::refreshHovering() {
		POINT win_mouse_pos;
		GetCursorPos(&win_mouse_pos);

		const cv::Point mouse_pos{win_mouse_pos.x, win_mouse_pos.y};

		if (results.empty())
			return;

		if (rect.contains(mouse_pos)) {
			if (is_hovering && hover_word && !hover_word->rect.empty() && cv::pointPolygonTest(
				    hover_word->rect,
				    mouse_pos,
				    false
			    ) > 0) {
				// caching previous rect (optimisation)
			} else {
				const auto intersect_iter = std::ranges::find_if(
					results,
					[&mouse_pos](const OCRBlock& block) -> bool {
						// returns positive (inside), negative (outside), or zero (on an edge) value
						if (block.poly.empty())
							return false;
						return cv::pointPolygonTest(block.poly, mouse_pos, false) > 0;
					}
				);
				if (intersect_iter != results.end()) {
					const auto word_iter = std::ranges::find_if(
						intersect_iter->results,
						[&mouse_pos](const OCRResultPacked& res) -> bool {
							// returns positive (inside), negative (outside), or zero (on an edge) value
							return cv::pointPolygonTest(res.rect, mouse_pos, false) > 0;
						}
					);
					if (word_iter != intersect_iter->results.end()) {
						is_hovering  = true;
						hover_word   = &*word_iter;
						hover_block  = &*intersect_iter;
						need_refresh = true;
					} else {
						is_hovering = false;
					}
				} else {
					// mouse isn't in any of the OCR areas
					is_hovering = false;
				}
			}
		} else {
			// if mouse is in the captured rect
			is_hovering = false;
		}
	}

	void TooltipWnd::updateLoop() {
		if (GetAsyncKeyState(VK_SHIFT) & (1 << 15)) {
			refreshHovering();
			if (is_hovering) {
				ShowWindow(hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(hwnd);
			} else {
				if (inited_web_view2) {
					ShowWindow(hwnd, SW_HIDE);
				}
				UpdateWindow(hwnd);
			}
		}
		if (need_refresh) {
			refreshWindow();
		}

		MSG msg;
		while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}
