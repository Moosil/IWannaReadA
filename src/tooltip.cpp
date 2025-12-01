#include "tooltip.h"

#include <ranges>

#include <gdiplus.h>
#include <dcomp.h>
#include <d2d1.h>
#include <wrl.h>
#include <dwrite.h>
#include <WebView2.h>
#pragma comment(lib,"gdiplus")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dcomp")
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

#include <filesystem>
#include <regex>
#include <Windowsx.h>
#include <spdlog/spdlog.h>
#include <opencv2/imgproc.hpp>
#include <utf8/cpp20.h>
#include <clip.h>
#include <nlohmann/json.hpp>

#include "implwebview2.h"
#include "log.h"
#include "task.h"
#include "util.h"


namespace ocr {
	TooltipWnd::~TooltipWnd() {
		mdict.reset();
	}

	HRESULT TooltipWnd::onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
		LPWSTR        msg = nullptr;
		const HRESULT err = args->get_WebMessageAsJson(&msg);
		log(
			err,
			"ICoreWebView2WebMessageReceivedEventArgs::get_WebMessageAsJson",
			ERR_LEVEL::WARN
		);
		if (SUCCEEDED(err) && msg) {
			const nlohmann::json parsed = nlohmann::json::parse(
				static_cast<std::u16string>(reinterpret_cast<const char16_t*>(msg))
			);
			if (parsed.contains("key")) {
				const auto key = parsed.at("key").get<std::string>();
				if (key == "mousedown") {
					// pass
				} else if (key == "contextmenu") {
					const int  x    = parsed.at("x").get<int>();
					const int  y    = parsed.at("y").get<int>();
					const auto word = parsed.at("word").get<std::string>();
					createContextMenu(x, y, word);
				} else {
					// pass
				}
			}
			CoTaskMemFree(msg);
		}
		return S_OK;
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
		int top;
		if (height < getTop(hover_word->rect)) {
			// window is too tall (it goes above top of screen)
			top = getTop(hover_word->rect);
		} else {
			// window can extend up and is below screen
			top = getBottom(hover_word->rect);
		}
		int        left;
		const auto screen_width = getScreenSize().first;
		if (getRight(hover_word->rect) + width > screen_width) {
			// window is too width (it goes past right of screen)
			left = static_cast<int>(screen_width) - width;
		} else {
			// window can extend right and is left of screen edge
			left = getLeft(hover_word->rect);
		}
		SetWindowPos(hwnd, HWND_TOPMOST, left, top - height, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void TooltipWnd::refreshWindow() {
		need_refresh = false;
		if (!is_hovering) {
			return;
		}

		const auto w_hover_text = utf8::utf8to16(hover_word->text);
		const auto [title_text_width, title_text_height] = getTextSize(w_hover_text);
		height = std::max(static_cast<int>(std::ceil(title_text_height)), min_height);
		width = std::max(static_cast<int>(std::ceil(title_text_width)), min_width);

		if (inited_web_view2) {
			const auto it = dictionary_data.find(hover_word->text);
			if (it == dictionary_data.end()) {
				initCurrDictHTML();
				if (!dict_loading_task) {
					dict_loading_task = std::make_unique<task<void> >(emptyDictStack());
					dict_loading_task->start();
				}
			} else {
				std::string total_website;
				auto&       [entries, sorted] = it->second;

				if (!sorted) {
					std::ranges::sort(
						entries,
						[](const DictionaryEntry& a, const DictionaryEntry& b) {
							return utf8::utf8to16(a.entry).size() > utf8::utf8to16(b.entry).size();
						}
					);
				}

				for (const auto [entry, html, webpage_width] : entries) {
					// Shadow DOM template to isolate duplicated HTML ids
					total_website += fmt::format(
						R"(<div id="{}"><template shadowrootmode="open"><style>{}</style>{}</template></div>)",
						entry,
						css_data,
						html
					);
					width = std::max(width, webpage_width);
				}
				total_website = "<html lang=\"en\"><head><title>" + hover_word->text + "</title></head><body>" + total_website
				                +
				                "</body></html>";

				const std::u16string total_website_u16 = utf8::utf8to16(total_website);
				const std::wstring   total_website_wstr(total_website_u16.begin(), total_website_u16.end());

				const HRESULT err = webview->NavigateToString(total_website_wstr.c_str());
				log(err, "ICoreWebView2.NavigateToString", ERR_LEVEL::FATAL);
			}
		}
	}

	void TooltipWnd::onWebsiteChanged() {
		const HRESULT script_err = webview->ExecuteScript(
			LR"(
				for (const host of document.body.children) {
					const shadow = host.shadowRoot;
					shadow.addEventListener('contextmenu', function (event) {
	                    let jsonObject = {
	                        key: 'contextmenu',
	                        x: event.screenX,
							y: event.screenY,
							word: host.id
	                    };
	                    window.chrome.webview.postMessage(jsonObject);
	                    event.preventDefault();
	                });
	                shadow.addEventListener('mousedown', function (event) {
	                    let jsonObject = {
	                        key: 'mousedown',
	                        x: event.screenX,
							y: event.screenY
	                    };
	                    window.chrome.webview.postMessage(jsonObject);
	                });
				}
            )",
			Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[this](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					log(errorCode, "ICoreWebView2ExecuteScriptCompletedHandler::Invoke", ERR_LEVEL::WARN);
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

		updateWindowSize();

		updateWindowPosition();
	}

	void TooltipWnd::createContextMenu(const int x, const int y, const std::string& phrase) const {
		HMENU menu = CreatePopupMenu();
		AppendMenu(menu, MF_STRING, 1, "Copy character");
		AppendMenu(menu, MF_STRING, 2, "Copy phrase");
		AppendMenu(menu, MF_STRING, 3, "Copy sentence");
		const int selected = TrackPopupMenu(
			menu,
			TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_NOANIMATION,
			x,
			y,
			0,
			hwnd,
			nullptr
		);
		DestroyMenu(menu);
		switch (selected) {
			case 1: {
				if (is_hovering) {
					clip::set_text(hover_word->text);
				}
				break;
			}
			case 2: {
				if (is_hovering) {
					clip::set_text(phrase);
				}
				break;
			}
			case 3: {
				if (is_hovering) {
					std::string copy{};
					for (const auto& [_, block_text] : hover_block->results) {
						copy.append(block_text);
					}
					clip::set_text(copy);
				}
				break;
			}
			default:
				break;
		}
	}

	void TooltipWnd::initCurrDictHTML() {
		const auto result_it = hover_block;
		std::string lookup_string;
		for (auto word_it = hover_word; word_it != result_it->results.end(); ++word_it) {
			lookup_string += word_it->text;

			const std::string dict_html = mdict->lookup(lookup_string);
			if (dict_html.empty()) {
				continue;
			}
			const std::string strip_dict_html = trim_copy(dict_html);
			const std::string static_lookup_string(lookup_string);
			dict_stack.emplace(static_lookup_string, strip_dict_html);
		}
	}

	task<void> TooltipWnd::emptyDictStack() {
		while (!dict_stack.empty()) {
			const auto [entry, strip_dict_html] = dict_stack.top();
			dict_stack.pop();
			const std::string dict_html_wrapped = "<html><head><style>" + css_data + "</style></head><body>" +
												  strip_dict_html + "</body></html>";

			const std::u16string dict_html_16 = utf8::utf8to16(dict_html_wrapped);
			const std::wstring   dict_html_w(dict_html_16.begin(), dict_html_16.end());
			co_await WebView2::Navigate{webview.get(), dict_html_w.c_str()};

			auto [err, widthStr] = co_await WebView2::ExecuteScript{
				webview.get(),
				LR"(
                    (() => {
                        const html = document.documentElement;
                        const body = document.body;
                        return Math.max(html.scrollWidth, body.scrollWidth);
                    })();
                )"
			};

			const int webpage_width = _wtoi(widthStr.c_str()) + scroll_bar_width;

			auto        start      = entry.begin();
			char32_t    first_char = utf8::next(start, entry.end());
			std::string first_char_str;
			utf8::append(first_char, std::back_inserter(first_char_str));
			dictionary_data[first_char_str].entries.emplace_back(entry, strip_dict_html, webpage_width);
			need_refresh = true;
		}
		dict_loading_task.reset();
	}

	void TooltipWnd::processOCRResults(
		const std::vector<OCRResult>& res,
		const cv::Point&              topleft,
		std::vector<OCRBlock>&        out
	) {
		out.clear();
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
			const auto       width         = *right - *left;
			const auto       height        = *bottom - *top;
			if (width > height) {
				auto                         it  = text.text.begin();
				const auto                   end = text.text.end();
				std::vector<OCRResultPacked> split_line{};
				for (const auto& [lower, upper] : text.char_lengths) {
					std::string utf16i;
					try {
						utf8::append(utf8::next(it, end), utf16i);
					} catch (const utf8::not_enough_room& _) {
						continue;
					}
					Poly2I char_rect{
						cv::Point{lerpi(moved_rect[0].x, moved_rect[1].x, lower), moved_rect[0].y},
						cv::Point{lerpi(moved_rect[0].x, moved_rect[1].x, upper), moved_rect[1].y},
						cv::Point{lerpi(moved_rect[3].x, moved_rect[2].x, upper), moved_rect[2].y},
						cv::Point{lerpi(moved_rect[3].x, moved_rect[2].x, lower), moved_rect[3].y}
					};
					split_line.emplace_back(char_rect, utf16i);
				}
				std::vector<int> intersections{};
				for (int i = 0; i < out.size(); i++) {
					if (out[i].horizontal && intersects(out[i].poly, moved_rect)) {
						intersections.push_back(i);
					}
				}
				if (intersections.empty()) {
					std::vector<cv::Point> poly{};
					poly.append_range(moved_rect);
					out.emplace_back(split_line, true, poly);
				} else {
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
				}
			} else {
				auto                         it  = text.text.begin();
				const auto                   end = text.text.end();
				std::vector<OCRResultPacked> split_line{};
				for (const auto& [lower, upper] : text.char_lengths) {
					std::string utf16i;
					try {
						utf8::append(utf8::next(it, end), utf16i);
					} catch (const utf8::not_enough_room& _) {
						continue;
					}
					Poly2I char_rect{
						cv::Point{moved_rect[0].x, lerpi(moved_rect[0].y, moved_rect[3].y, lower)},
						cv::Point{moved_rect[1].x, lerpi(moved_rect[1].y, moved_rect[2].y, lower)},
						cv::Point{moved_rect[2].x, lerpi(moved_rect[1].y, moved_rect[2].y, upper)},
						cv::Point{moved_rect[3].x, lerpi(moved_rect[0].y, moved_rect[3].y, upper)}
					};
					split_line.emplace_back(char_rect, utf16i);
				}
				std::vector<int> intersections{};
				for (int i = 0; i < out.size(); i++) {
					if (!out[i].horizontal && intersects(out[i].poly, moved_rect)) {
						intersections.push_back(i);
					}
				}
				if (intersections.empty()) {
					std::vector<cv::Point> poly{};
					poly.append_range(moved_rect);
					out.emplace_back(split_line, false, poly);
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
		std::thread(
			[&wnd, res, rect]() {
				processOCRResults(res, cv::Point{rect.x, rect.y}, wnd->results);
			}
		).detach();

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

		wnd->initDirectWrite();

		wnd->initDictionary(dict_folder_path);

		return wnd;
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
			RECT{0, title_bar_height, width, height},
			[this](ICoreWebView2Controller* controller, ICoreWebView2* wv) {
				if (!controller || !wv) {
					return;
				}
				wv_controller    = controller;
				webview          = wv;
				inited_web_view2 = true;


				const HRESULT nav_subscribe_err = webview->add_NavigationCompleted(
					Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
						[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
							onWebsiteChanged();
							return S_OK;
						}
					).Get(),
					nullptr
				);
				log(nav_subscribe_err, "ICoreWebView2::add_NavigationCompleted", ERR_LEVEL::WARN);
				if (!is_hovering) {
					ShowWindow(hwnd, SW_HIDE);
				}
			}
		);
		wv_init->try_init_env();
	}

	bool TooltipWnd::initDirectWrite() {
		auto err = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d1_factory);
		log(err, "D2D1CreateFactory", ERR_LEVEL::FATAL);
		if (FAILED(err))
			return false;

		err = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&direct_write_factory)
		);
		log(err, "ID2D1Factory.DWriteCreateFactory", ERR_LEVEL::FATAL);
		if (FAILED(err))
			return false;

		err = direct_write_factory->CreateTextFormat(
			L"KaiTi",
			nullptr,
			DWRITE_FONT_WEIGHT_BOLD,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			24.0f,
			L"zh-CN",
			&direct_write_text_format
		);
		log(err, "IDWriteFactory.CreateTextFormat", ERR_LEVEL::FATAL);
		if (FAILED(err))
			return false;

		const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		);
		const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
			hwnd,
			D2D1::SizeU(static_cast<unsigned int>(width), static_cast<unsigned int>(height))
		);

		err = d2d1_factory->CreateHwndRenderTarget(rtProps, hwndProps, &render_target);
		log(err, "ID2D1Factory.CreateHwndRenderTarget", ERR_LEVEL::FATAL);
		if (FAILED(err))
			return false;

		err = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush);
		log(err, "ID2D1HwndRenderTarget.CreateSolidColorBrush", ERR_LEVEL::FATAL);
		if (FAILED(err))
			return false;

		return true;
	}

	void TooltipWnd::cleanupDirectWrite() {
		direct_write_text_format.reset();
		brush.reset();
		render_target.reset();
		direct_write_factory.reset();
		d2d1_factory.reset();
	}

	void TooltipWnd::updateLoop() {
		if (GetAsyncKeyState(VK_SHIFT) & (1 << 15)) {
			POINT win_mouse_pos;
			GetCursorPos(&win_mouse_pos);

			const cv::Point mouse_pos{win_mouse_pos.x, win_mouse_pos.y};
			if (rect.contains(mouse_pos)) {
				if (is_hovering && cv::pointPolygonTest(hover_word->rect, mouse_pos, false) > 0) {
					// caching previous rect in case (optimisation)
				} else {
					const auto intersect_iter = std::ranges::find_if(
						results,
						[&mouse_pos](const auto& res) {
							// returns positive (inside), negative (outside), or zero (on an edge) value
							return cv::pointPolygonTest(res.poly, mouse_pos, false) > 0;
						}
					);
					if (intersect_iter != results.end()) {
						const auto word_iter = std::ranges::find_if(
							intersect_iter->results,
							[&mouse_pos](const auto& res) {
								// returns positive (inside), negative (outside), or zero (on an edge) value
								return cv::pointPolygonTest(res.rect, mouse_pos, false) > 0;
							}
						);
						if (word_iter != intersect_iter->results.end()) {
							is_hovering     = true;
							hover_word = word_iter;
							hover_block     = intersect_iter;
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

	std::pair<float, float> TooltipWnd::getTextSize(
		const std::u16string& w_hover_text,
		const float           p_width,
		const float           p_height
	) const {
		wil::com_ptr<IDWriteTextLayout> text_layout;
		direct_write_factory->CreateTextLayout(
			reinterpret_cast<const wchar_t*>(w_hover_text.c_str()),
			static_cast<UINT32>(w_hover_text.length()),
			direct_write_text_format.get(),
			p_width,
			p_height,
			&text_layout
		);

		DWRITE_TEXT_METRICS text_metrics{};
		text_layout->GetMetrics(&text_metrics);
		text_layout->Release();

		return {text_metrics.width, text_metrics.height};
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
				createContextMenu(x, y, hover_word->text);
				break;
			}
			case WM_SIZE: {
				width  = LOWORD(lparam);
				height = HIWORD(lparam);

				if (wv_controller) {
					RECT rc;
					GetClientRect(hwnd, &rc);
					rc.top += title_bar_height;
					const HRESULT err = wv_controller->put_Bounds(rc);
					log(err, "ICoreWebView2Controller.put_Bounds", ERR_LEVEL::WARN);
				};
				if (render_target) {
					const HRESULT err = render_target->Resize(
						D2D1::SizeU(static_cast<unsigned int>(width), static_cast<unsigned int>(height))
					);
					log(err, "ID2D1HwndRenderTarget.Resize", ERR_LEVEL::WARN);
				}
				break;
			}
			case WM_PAINT: {
				if (is_hovering) {
					PAINTSTRUCT ps;
					BeginPaint(hwnd, &ps);

					render_target->BeginDraw();
					render_target->Clear(D2D1::ColorF(D2D1::ColorF::White));

					const std::u16string w_hover_text = utf8::utf8to16(hover_word->text);
					direct_write_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					render_target->DrawText(
						reinterpret_cast<const wchar_t*>(w_hover_text.c_str()),
						static_cast<UINT32>(w_hover_text.length()),
						direct_write_text_format.get(),
						D2D1::RectF(0, 0, static_cast<float>(width), static_cast<float>(height)),
						brush.get()
					);

					render_target->EndDraw();

					EndPaint(hwnd, &ps);
					return 0;
				}
				break;
			}
			case WM_DESTROY: {
				cleanupDirectWrite();
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
}
