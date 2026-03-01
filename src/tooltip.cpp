#include "tooltip.h"

#include <clip.h>
#include <filesystem>
#include <ranges>
#include <WebView2.h>
#include <Windowsx.h>
#include <cpp-pinyin/G2pglobal.h>
#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>
#include <wrl/event.h>

#include "implwebview2.h"
#include "log.h"
#include "util.h"


namespace ocr {
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
					utf8ToWide(std::format(html_skeleton, css_data)).c_str()
				);
				log(nav_err, "ICoreWebView2::NavigateToString", ERR_LEVEL::FATAL);

				if (!is_hovering) {
					ShowWindow(hwnd, SW_HIDE);
				}
			}
		);
		wv_init->try_init_env();
	}

	void TooltipWnd::initDictionary(const std::string& dict_string_path) {
		const std::filesystem::path dict_folder_path_path = dict_string_path;
		std::filesystem::path       dict_file_path{dict_folder_path_path};
		dict_file_path /= dict_file_path.filename();
		dict_file_path += ".mdx";
		for (const auto& dir_item : std::filesystem::directory_iterator{dict_folder_path_path}) {
			if (dir_item.path().extension() == ".css") {
				std::ifstream     in(dir_item.path());
				const std::string contents(
					(std::istreambuf_iterator(in)),
					std::istreambuf_iterator<char>()
				);
				css_data += contents;
			}
		}

		mdict = std::make_unique<mdict::Mdict>(dict_file_path.string());
		mdict->init();
	}

	void TooltipWnd::initCurrDictHTML() {
		if (!hover_word || !hover_block || results.empty())
			return;

		std::string       lookup_string;
		const std::string first_char = hover_word->text;
		dictionary_data[first_char]  = {};
		for (const auto* curr = hover_word; curr != hover_block->results.data() + hover_block->results.size(); ++curr) {
			lookup_string += curr->text;

			if (const std::string dict_html = mdict->lookup(lookup_string); !dict_html.empty()) {
				const std::string strip_dict_html = trim_copy(dict_html);

				dictionary_data[first_char].entries.emplace_back(lookup_string, strip_dict_html);
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
			initCurrDictHTML();
			need_refresh = true;
			return;
		}
		// if (it != dictionary_data.end()) {
		// 	spdlog::info(it->first);
		// }

		if (it->second.entries.empty()) {
			const HRESULT err = webview->ExecuteScript(
				reset_webpage_script,
				Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					// ReSharper disable once CppParameterMayBeConst
					[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
						log(errorCode, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
						return S_OK;
					}
				).Get()
			);
			log(err, "ICoreWebView2::ExecuteScript", ERR_LEVEL::WARN);
			return;
		}

		std::string script;
		auto&       dict_data = it->second; {
			int i = 0;
			for (; i < dict_data.entries.size(); i++) {
				// Shadow DOM template to isolate duplicated HTML ids
				script += std::format(
					fill_webpage_script,
					i,
					hover_word->text,
					dict_data.entries[i].entry,
					getSentence(hover_block),
					dict_data.entries[i].entry_html
				);
			}
			for (; i < 5; i++) {
				script += std::format(
					fill_webpage_script,
					i,
					"",
					"",
					"",
					""
				);
			}
		}
		width = std::max(min_width, max_webpage_width);

		if (dict_data.height == -1) {
			height = min_height;
			updateWindowSize();
			updateWindowPosition();
			const HRESULT err0 = webview->ExecuteScript(
				utf8ToWide(script).c_str(),
				Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					// ReSharper disable once CppParameterMayBeConst
					[this, &dict_data](HRESULT errorCode0, LPCWSTR resultObjectAsJson) -> HRESULT {
						log(errorCode0, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
						const HRESULT err1 = webview->ExecuteScript(
							get_height_script,
							Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
								// ReSharper disable CppParameterMayBeConst
								[this, &dict_data](HRESULT errorCode1, LPCWSTR result) -> HRESULT {
									// ReSharper restore CppParameterMayBeConst
									log(errorCode1, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
									if (SUCCEEDED(errorCode1) && result && is_hovering) {
										const int calc_webpage_height = _wtoi(result);
										// for when width was used ----------v
										//max_webpage_width            = std::max(calc_webpage_height, max_webpage_width);

										height = std::clamp(
											std::min(
												calc_webpage_height,
												std::max(
													getTop(hover_word->rect),
													getScreenSize().second - getBottom(hover_word->rect)
												)
											),
											min_height,
											max_height
										);
										dict_data.height = calc_webpage_height;
										spdlog::info(
											"entry: {}, height: {}",
											dict_data.entries[0].entry,
											calc_webpage_height
										);
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

	void TooltipWnd::onNavigationComplete() {
		const HRESULT script_err = webview->ExecuteScript(
			add_context_menu_script,
			Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				// ReSharper disable once CppParameterMayBeConst
				[this](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					log(errorCode, "ExecuteScript::Invoke", ERR_LEVEL::WARN);
					EventRegistrationToken token = {};

					const HRESULT add_msg_err = webview->add_WebMessageReceived(
						Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
							this,
							&TooltipWnd::onWebMessageReceived
						).Get(),
						&token
					);
					log(add_msg_err, "ICoreWebView2::add_WebMessageReceived", ERR_LEVEL::WARN);
					return S_OK;
				}
			).Get()
		);
		log(script_err, "ICoreWebView2::ExecuteScript", ERR_LEVEL::FATAL);
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
					json.at("sentence")
				);
			} else {
				//mousedown
			}
		}
		return S_OK;
	}

	void TooltipWnd::createContextMenu(
		const int          x,
		const int          y,
		const std::string& character,
		const std::string& phrase,
		const std::string& sentence
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
				auto        find_pos     = sentence.find(phrase);
				std::string sentence_add = std::format(
					"{}{{{{c1::{}}}}}{}",
					sentence.substr(0, find_pos),
					sentence.substr(find_pos, phrase.length()),
					sentence.substr(find_pos + phrase.length())
				);

				const auto pinyin = g2p_man->hanziToPinyin(phrase);

				anki.add_note(phrase, pinyin.toStdStr(), "", sentence_add);
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
					createContextMenu(x, y, hover_word->text, hover_word->text, getSentence(hover_block));
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
		mdict.reset();
	}

	std::unique_ptr<TooltipWnd> TooltipWnd::initTooltip(
		const std::vector<OCRResult>& res,
		const cv::Rect&               rect,
		const std::string&            dict_folder_path
	) {
		auto wnd = std::make_unique<TooltipWnd>();
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

		wnd->rect = rect;
		processOCRResults(res, cv::Point{wnd->rect.x, wnd->rect.y}, wnd->results);

		constexpr int style           = WS_POPUP;
		constexpr int extended_styles = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
		wnd->width                    = min_width;
		wnd->height                   = min_height;

		wnd->hwnd = CreateWindowEx(
			extended_styles,
			className.c_str(),
			"tt",
			style,
			0,
			0,
			wnd->width,
			wnd->height,
			nullptr,
			nullptr,
			GetModuleHandle(nullptr),
			wnd.get()
		);

		wnd->initDictionary(dict_folder_path);

		log(SetWindowDisplayAffinity(wnd->hwnd, WDA_EXCLUDEFROMCAPTURE), "SetWindowDisplayAffinity");

		const auto applicationDirPath = std::filesystem::current_path() / "dict";
		Pinyin::setDictionaryPath(applicationDirPath);
		wnd->g2p_man = std::make_unique<Pinyin::Pinyin>();

		return wnd;
	}

	// 40ms
	void TooltipWnd::updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect) {
		processOCRResults(new_res, cv::Point{rect.x, rect.y}, results);

		rect        = new_rect;
		hover_block = nullptr;
		hover_word  = nullptr;
		refreshHovering();
		UpdateWindow(hwnd);
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
