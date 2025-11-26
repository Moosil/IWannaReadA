#include "tooltip.h"

#include <ranges>

#include <gdiplus.h>
#include <dcomp.h>
#include <wrl.h>
#pragma comment(lib,"gdiplus")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dcomp")

#include <filesystem>
#include <spdlog/spdlog.h>
#include <opencv2/imgproc.hpp>
#include <utf8/cpp20.h>
#include <xmlutils.h>

#include "log.h"
#include "util.h"


namespace ocr {
	TooltipWnd::~TooltipWnd() {
		mdict.reset();
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

	void TooltipWnd::startDictLoading() {
		dict_init_nav_tokens.resize(results.size());
		loadDictEntry(0);
	}

	void TooltipWnd::loadDictEntry(const int iter) {
		if (iter >= results.size()) {
			inited_dictionary = true;

			if (!is_hovering) {
				ShowWindow(hwnd, SW_HIDE);
				UpdateWindow(hwnd);
			}
			return;
		}

		std::string dictionary_html = mdict->lookup(results[iter].text);

		if (!dictionary_html.contains("<body>")) {
			dictionary_html = "<body>" + dictionary_html + "</body>";
		}
		if (!dictionary_html.contains("<html>")) {
			dictionary_html = "<html>" + dictionary_html + "</html>";
		}

		dictionary_html += fmt::format("<style>{}</style>", css_data);
		const std::u16string dict_html_16 = utf8::utf8to16(dictionary_html);
		const std::wstring   dict_html_w(dict_html_16.begin(), dict_html_16.end());

		HRESULT              nav_err = webview->NavigateToString(dict_html_w.c_str());
		log(nav_err, "ICoreWebView2.NavigateToString", ERR_LEVEL::FATAL);

		EventRegistrationToken& tokenRef = dict_init_nav_tokens[iter];
		nav_err = webview->add_NavigationCompleted(
			Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
				[this, iter, dict_html_w, &tokenRef](
					ICoreWebView2*                             sender,
					ICoreWebView2NavigationCompletedEventArgs* args
				) -> HRESULT {
					const HRESULT exec_err = webview->ExecuteScript(
						LR"(
							(function() {
							  const html = document.documentElement;
							  const body = document.body;

							  const minWidth = Math.max(html.scrollWidth, body.scrollWidth);

							  return minWidth;
							})();
						    )",
						Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
							[this, iter, dict_html_w, &tokenRef, sender](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
								spdlog::info("Loaded dict data for {}", iter);
								log(errorCode, "ICoreWebView2ExecuteScriptCompletedHandler.Invoke", ERR_LEVEL::WARN);

								if (SUCCEEDED(errorCode)) {
									const int webview_width = _wtoi(resultObjectAsJson) + scroll_bar_width;

									dictionary_data.insert({
										results[iter].text,
										DictionaryData{dict_html_w, webview_width}
									});

									const HRESULT rem_err = sender->remove_NavigationCompleted(tokenRef);
									log(rem_err, "ICoreWebView2.remove_NavigationCompleted", ERR_LEVEL::WARN);

									loadDictEntry(iter + 1);
								}
								return S_OK;
							}
						).Get()
					);
					log(exec_err, "ICoreWebView2.ExecuteScript", ERR_LEVEL::WARN);
					return S_OK;
				}
			).Get(),
			&tokenRef
		);
		log(nav_err, "ICoreWebView2.add_NavigationCompleted", ERR_LEVEL::WARN);
	}

	std::vector<OCRResultPacked> TooltipWnd::processOCRResults(
		const std::vector<OCRResult>& res,
		const cv::Point&              topleft,
		const bool                    separate_characters
	) {
		std::vector<OCRResultPacked> res_packed;
		if (separate_characters) {
			for (const auto& [rect, text] : res) {
				const std::array xs            = {rect.rect[0].x, rect.rect[1].x, rect.rect[2].x, rect.rect[3].x};
				const std::array ys            = {rect.rect[0].y, rect.rect[1].y, rect.rect[2].y, rect.rect[3].y};
				const auto       [left, right] = std::minmax_element(xs.begin(), xs.end());
				const auto       [top, bottom] = std::minmax_element(ys.begin(), ys.end());
				const auto       width         = *right - *left;
				const auto       height        = *bottom - *top;
				if (width > height) {
					auto       it  = text.text.begin();
					const auto end = text.text.end();
					for (const auto& [lower, upper] : text.char_lengths) {
						Poly2I char_rect{
							cv::Point{lerpi(rect.rect[0].x, rect.rect[1].x, lower), rect.rect[0].y} + topleft,
							cv::Point{lerpi(rect.rect[0].x, rect.rect[1].x, upper), rect.rect[1].y} + topleft,
							cv::Point{lerpi(rect.rect[3].x, rect.rect[2].x, upper), rect.rect[2].y} + topleft,
							cv::Point{lerpi(rect.rect[3].x, rect.rect[2].x, lower), rect.rect[3].y} + topleft
						};
						std::string utf16i;
						utf8::append(utf8::next(it, end), utf16i);
						res_packed.emplace_back(char_rect, utf16i);
					}
				} else {
					auto       it  = text.text.begin();
					const auto end = text.text.end();
					for (const auto& [lower, upper] : text.char_lengths) {
						Poly2I char_rect{
							cv::Point{rect.rect[0].x, lerpi(rect.rect[0].x, rect.rect[3].x, lower)} + topleft,
							cv::Point{rect.rect[1].x, lerpi(rect.rect[1].x, rect.rect[2].x, lower)} + topleft,
							cv::Point{rect.rect[2].x, lerpi(rect.rect[1].x, rect.rect[2].x, lower)} + topleft,
							cv::Point{rect.rect[3].x, lerpi(rect.rect[0].x, rect.rect[3].x, lower)} + topleft
						};
						std::string utf16i;
						utf8::append(utf8::next(it, end), utf16i);
						res_packed.emplace_back(char_rect, utf16i);
					}
				}
			}
		} else {
			res_packed.reserve(res.size());

			for (const auto& [rect, text] : res) {
				Poly2I new_rect;
				std::ranges::copy(
					std::views::iota(0, 4) | std::views::transform(
						[&rect, &topleft](const int i) -> cv::Point2i {
							return rect.rect[i] + topleft;
						}
					),
					new_rect.begin()
				);
				res_packed.emplace_back(new_rect, text.text);
			}
		}
		return res_packed;
	}

	void TooltipWnd::loadDictionary(const std::string& dict_string_path) {
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
			wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
			if (!RegisterClassA(&wc)) {
				spdlog::error("registering window class failed");
				return nullptr;
			}
			isInitialised = true;
		}

		wnd->rect    = rect;
		wnd->results = processOCRResults(res, cv::Point{rect.x, rect.y}, true);

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

		wnd->loadDictionary(dict_folder_path);

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
				controller->AddRef();
				wv->AddRef();
				wv_controller    = controller;
				webview          = wv;
				inited_web_view2 = true;

				startDictLoading();
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

	void TooltipWnd::cleanupDirectWrite() const {
		if (direct_write_text_format)
			direct_write_text_format->Release();
		if (brush)
			brush->Release();
		if (render_target)
			render_target->Release();
		if (direct_write_factory)
			direct_write_factory->Release();
		if (d2d1_factory)
			d2d1_factory->Release();
	}

	void TooltipWnd::updateLoop() {
		POINT win_mouse_pos;
		GetCursorPos(&win_mouse_pos);
		SetWindowPos(hwnd, HWND_TOPMOST, win_mouse_pos.x, win_mouse_pos.y - height, -1, -1, SWP_NOSIZE | SWP_NOZORDER);

		const cv::Point mouse_pos{win_mouse_pos.x, win_mouse_pos.y};
		if (!rect.contains(mouse_pos)) {
			// if mouse is in the captured rect
			is_hovering = false;
		} else if (is_hovering && cv::pointPolygonTest(prev_hover_rect, mouse_pos, false) > 0) {
			// caching previous rect in case (optimisation)
		} else {
			is_hovering = false;
			for (const auto& [text_rect, text] : results) {
				// returns positive (inside), negative (outside), or zero (on an edge) value
				if (cv::pointPolygonTest(text_rect, mouse_pos, false) > 0) {
					// inside polygon
					is_hovering     = true;
					prev_hover_rect = text_rect;
					hover_text      = text;
					break;
				}
			}
			if (!is_hovering) {
				// for logging ONLY
				// hover_text = "";
			} else {
				const auto w_hover_text = utf8::utf8to16(hover_text);
				const auto [title_text_width, title_text_height] = getTextSize(w_hover_text);
				height = std::max(static_cast<int>(std::ceil(title_text_height)), min_height);
				width = std::max(static_cast<int>(std::ceil(title_text_width)), min_width);

				if (inited_web_view2 && inited_dictionary) {
					const auto    [entry, webpage_width] = dictionary_data[hover_text];
					const HRESULT err                    = webview->NavigateToString(entry.c_str());
					log(err, "ICoreWebView2.NavigateToString", ERR_LEVEL::FATAL);

					width = std::max(width, webpage_width);
				}
				updateWindowSize();
			}
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

		MSG msg;
		while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	std::tuple<float, float> TooltipWnd::getTextSize(
		const std::u16string& w_hover_text,
		const float           p_width,
		const float           p_height
	) const {
		IDWriteTextLayout* text_layout;
		/*auto err = */
		direct_write_factory->CreateTextLayout(
			reinterpret_cast<const wchar_t*>(w_hover_text.c_str()),
			static_cast<UINT32>(w_hover_text.length()),
			direct_write_text_format,
			p_width,
			p_height,
			&text_layout
		);

		DWRITE_TEXT_METRICS text_metrics{};
		/*err = */
		text_layout->GetMetrics(&text_metrics);

		return {text_metrics.width, text_metrics.height};
	}

	LRESULT TooltipWnd::wndProc(UINT msg, WPARAM wparam, LPARAM lparam) {
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
			case WM_SIZE: {
				width  = LOWORD(lparam);
				height = HIWORD(lparam);

				if (webview) {
					const HRESULT err = wv_controller->put_Bounds(RECT{0, title_bar_height, width, height});
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

					const std::u16string w_hover_text = utf8::utf8to16(hover_text);
					direct_write_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					render_target->DrawText(
						reinterpret_cast<const wchar_t*>(w_hover_text.c_str()),
						static_cast<UINT32>(w_hover_text.length()),
						direct_write_text_format,
						D2D1::RectF(0, 0, static_cast<float>(width), static_cast<float>(height)),
						brush
					);

					render_target->EndDraw();

					EndPaint(hwnd, &ps);
					return 0;
				}
				break;
			}
			case WM_DESTROY: {
				if (wv_controller) {
					this->wv_controller->Release();
					wv_controller = nullptr;
				}
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

	LRESULT CALLBACK TooltipWnd::wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
