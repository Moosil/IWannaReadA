#include "tooltip.h"

#include <ranges>

#include <gdiplus.h>
#include <dwmapi.h>
#include <dcomp.h>
#pragma comment(lib,"gdiplus")
#pragma comment(lib,"dwmapi")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dcomp")

#include <filesystem>
#include <spdlog/spdlog.h>
#include <opencv2/imgproc.hpp>
#include <utf8/cpp20.h>
#include <xmlutils.h>

#include "util.h"


namespace ocr {
	TooltipWnd::~TooltipWnd() {
		mdict.reset();
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

	std::unique_ptr<TooltipWnd> TooltipWnd::initTooltip(
		const std::vector<OCRResult>& res,
		const cv::Point&              topleft,
		const std::string&            dict_path
	) {
		auto wnd = std::make_unique<TooltipWnd>();
		if (!isInitialised) {
			WNDCLASS wc{};
			wc.lpfnWndProc   = &wndProcSetup;
			wc.hInstance     = GetModuleHandle(nullptr);
			wc.lpszClassName = className.c_str();
			wc.hbrBackground = nullptr;
			if (!RegisterClassA(&wc)) {
				spdlog::error("registering window class failed");
				return nullptr;
			}
			isInitialised = true;
		}

		wnd->results = processOCRResults(res, topleft, true);

		constexpr int style           = WS_OVERLAPPEDWINDOW;
		constexpr int extended_styles = WS_EX_TOPMOST;
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

		wnd->mdict = std::make_unique<mdict::Mdict>(dict_path);
		wnd->mdict->init();

		return wnd;
	}

	bool TooltipWnd::initWebView2() {
		auto err = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (SUCCEEDED(err)) {
			spdlog::info("setup com");
		} else {
			spdlog::critical("failed setup com");
		}
		wchar_t* version;
		if (FAILED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &version))) {
			spdlog::critical("failed to find WebView2 runtime");
			return false;
		} else {
			CoTaskMemFree(version);
		}
		wv_init = std::make_unique<WebView2::Impl>(
			hwnd,
			width,
			height,
			[this](ICoreWebView2Controller* controller, ICoreWebView2* wv) {
				if (!controller || !wv) {
					return;
				}
				controller->AddRef();
				wv->AddRef();
				wv_controller  = controller;
				webview        = wv;
				initedWebView2 = true;
			}
		);
		wv_init->try_init_env();

		return true;
	}

	bool TooltipWnd::initDirectWrite() {
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d1_factory)))
			return false;

		if (FAILED(
			DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(&direct_write_factory))
		))
			return false;

		const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		);
		const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
			hwnd,
			D2D1::SizeU(static_cast<unsigned int>(width), static_cast<unsigned int>(height))
		);

		if (FAILED(d2d1_factory->CreateHwndRenderTarget(rtProps, hwndProps, &render_target)))
			return false;

		if (FAILED(render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush)))
			return false;

		if (FAILED(
			direct_write_factory->CreateTextFormat(
				L"KaiTi",
				nullptr,
				DWRITE_FONT_WEIGHT_BOLD,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				24.0f,
				L"zh-CN",
				&direct_write_text_format)
		))
			return false;

		return true;
	}

	void TooltipWnd::cleanupDirectWrite() {
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
		if (win_mouse_pos.x != prev_hover_point.x && win_mouse_pos.y != prev_hover_point.y) {
			prev_hover_point = win_mouse_pos;
			const cv::Point mouse_pos{win_mouse_pos.x, win_mouse_pos.y};
			SetWindowPos(hwnd, HWND_TOPMOST, mouse_pos.x, mouse_pos.y - height, -1, -1, SWP_NOSIZE | SWP_NOZORDER);

			if (cv::pointPolygonTest(prev_hover_rect, mouse_pos, false) > 1) {
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
				if (is_hovering) {
					const auto w_hover_text = utf8::utf8to16(hover_text);
					const auto [title_text_width, title_text_height] = getTextSize(w_hover_text);
					width = std::max(static_cast<int>(std::ceil(title_text_width)), min_width);
					width = std::max(static_cast<int>(std::ceil(title_text_height)), min_height);

					dictionary_html = mdict->lookup(hover_text);

					SetWindowPos(
						hwnd,
						HWND_TOPMOST,
						0,
						0,
						width,
						height,
						SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW
					);
					render_target->Resize(
						D2D1::SizeU(static_cast<unsigned int>(width), static_cast<unsigned int>(height))
					);
					InvalidateRect(hwnd, nullptr, FALSE);
				}
			}
			if (is_hovering) {
				ShowWindow(hwnd, SW_SHOWNOACTIVATE);
				UpdateWindow(hwnd);
			} else {
				if (initedWebView2) {
					spdlog::info("inited web view 2 supposedly");
					ShowWindow(hwnd, SW_HIDE);
				}
				UpdateWindow(hwnd);
			}
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
		direct_write_factory->CreateTextLayout(
			reinterpret_cast<const wchar_t*>(w_hover_text.c_str()),
			static_cast<UINT32>(w_hover_text.length()),
			direct_write_text_format,
			p_width,
			p_height,
			&text_layout
		);

		DWRITE_TEXT_METRICS text_metrics{};
		text_layout->GetMetrics(&text_metrics);

		return {text_metrics.width, text_metrics.height};
	}

	LRESULT TooltipWnd::wndProc(UINT msg, WPARAM wparam, LPARAM lparam) {
		switch (msg) {
			case WM_CREATE: {
				constexpr DWM_SYSTEMBACKDROP_TYPE backdrop_material = DWMSBT_TRANSIENTWINDOW;
				const HRESULT                     backdrop_err      = DwmSetWindowAttribute(
					hwnd,
					DWMWA_SYSTEMBACKDROP_TYPE,
					&backdrop_material,
					sizeof(backdrop_material)
				);
				spdlog::info("{} set backdrop material", (FAILED(backdrop_err) ? "successfully" : "failed to"));

				constexpr DWM_WINDOW_CORNER_PREFERENCE corner_round     = DWMWCP_ROUNDSMALL;
				const auto                             corner_round_err = DwmSetWindowAttribute(
					hwnd,
					DWMWA_WINDOW_CORNER_PREFERENCE,
					&corner_round,
					sizeof(corner_round)
				);
				spdlog::info("{} set corner rounding", (FAILED(corner_round_err) ? "successfully" : "failed to"));


				if (!initedWebView2) {
					ShowWindow(hwnd, SW_SHOWNOACTIVATE);
					if (!initWebView2()) {
						spdlog::critical("failed to initialize webview2");
					};
				}

				for (const auto& [text_rect, text] : results) {
					spdlog::info(
						"{}: [({}, {}), ({}, {}), ({}, {}), ({}, {})]",
						text,
						text_rect[0].x,
						text_rect[0].y,
						text_rect[1].x,
						text_rect[1].y,
						text_rect[2].x,
						text_rect[2].y,
						text_rect[3].x,
						text_rect[3].y
					);
				}
				break;
			}
			case WM_KEYDOWN: {
				if (wparam == VK_ESCAPE) {
					// do something
				}
				break;
			}
			case WM_SIZE: {
				if (webview != nullptr) {
					RECT bounds;
					GetClientRect(hwnd, &bounds);
					wv_controller->put_Bounds(bounds);
				};
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

					std::wstring dict_html_w(dictionary_html.begin(), dictionary_html.end());
					if (initedWebView2) {
						spdlog::info("inited web view 2 supposedly");
						webview->NavigateToString(dict_html_w.c_str());
					}

					render_target->EndDraw();

					EndPaint(hwnd, &ps);
					return 0;
				}
			}
			case WM_DESTROY: {
				if (wv_controller)
					this->wv_controller->Release();
				cleanupDirectWrite();
				break;
			}
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
