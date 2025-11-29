#include "tooltip.h"

#include <ranges>

#include <gdiplus.h>
#include <dcomp.h>
#include <wrl.h>
#pragma comment(lib,"gdiplus")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dcomp")

#include <filesystem>
#include <regex>
#include <spdlog/spdlog.h>
#include <opencv2/imgproc.hpp>
#include <utf8/cpp20.h>

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

	void TooltipWnd::refreshWindow() {
		const auto w_hover_text = utf8::utf8to16(hover_text);
		const auto [title_text_width, title_text_height] = getTextSize(w_hover_text);
		height = std::max(static_cast<int>(std::ceil(title_text_height)), min_height);
		width = std::max(static_cast<int>(std::ceil(title_text_width)), min_width);

		if (inited_web_view2 && inited_dictionary) {
			std::string total_website;

			const auto& [entries, sorted] = dictionary_data[hover_text];

			std::vector<DictionaryEntry> sorted_entries = dictionary_data[hover_text].entries;
			if (!sorted) {
				std::ranges::sort(
					sorted_entries,
					[](const DictionaryEntry& a, const DictionaryEntry& b) {
						return utf8::utf8to16(a.words).size() > utf8::utf8to16(b.words).size();
					}
				);
			}

			for (const auto [html, webpage_width, word] : sorted_entries) {
				// Shadow DOM template to isolate duplicated HTML ids
				total_website += "<div><template shadowrootmode=\"open\"><style>" + css_data + "</style>" + html
						+ "</template></div>";
				width = std::max(width, webpage_width);
			}
			total_website = "<html lang=\"en\"><head><title>" + hover_text + "</title></head><body>" + total_website +
			                "</body></html>";

			const std::u16string total_website_u16 = utf8::utf8to16(total_website);
			const std::wstring   total_website_wstr(total_website_u16.begin(), total_website_u16.end());

			const HRESULT err = webview->NavigateToString(total_website_wstr.c_str());
			log(err, "ICoreWebView2.NavigateToString", ERR_LEVEL::FATAL);
		}
		updateWindowSize();

		int top;
		if (height < getTop(prev_hover_rect)) {
			// window is too tall (it goes above top of screen)
			top = getTop(prev_hover_rect);
		} else {
			// window can extend up and is below screen
			top = getBottom(prev_hover_rect);
		}
		int        left;
		const auto [screen_width, _] = getScreenSize();
		if (getRight(prev_hover_rect) + width > screen_width) {
			// window is too width (it goes past right of screen)
			left = static_cast<int>(screen_width) - width;
		} else {
			// window can extend right and is left of screen edge
			left = getLeft(prev_hover_rect);
		}
		SetWindowPos(hwnd, HWND_TOPMOST, left, top - height, -1, -1, SWP_NOSIZE | SWP_NOZORDER);
	}

	void TooltipWnd::startDictLoading() {
		dict_init_nav_tokens.resize(results.size());
		std::size_t max_length = 0;
		for (const auto& res : results) {
			max_length = std::max(max_length, res.results.size());
		}
		loadDictEntry(0, 0, max_length);
	}

	void TooltipWnd::loadDictEntry(
		std::size_t       iter1,
		std::size_t       iter2,
		const std::size_t max_length,
		const std::size_t length
	) {
		if (length > max_length) {
			inited_dictionary = true;

			if (!is_hovering) {
				ShowWindow(hwnd, SW_HIDE);
				UpdateWindow(hwnd);
			}
			return;
		}

		if (iter2 + length - 1 >= results[iter1].results.size() && iter1 + 1 >= results.size()) {
			loadDictEntry(0, 0, max_length, length + 1);
			return;
		}

		while (iter2 + length - 1 >= results[iter1].results.size()) {
			iter2 = 0;
			iter1++;

			if (iter2 + length - 1 >= results[iter1].results.size() && iter1 + 1 >= results.size()) {
				loadDictEntry(0, 0, max_length, length + 1);
				return;
			}
		}

		const std::string lookup_string = std::views::iota(iter2, iter2 + length)
		                                  | std::ranges::views::transform(
			                                  [this, iter1](const std::size_t i) -> std::string {
				                                  return results[iter1].results[i].text;
			                                  }
		                                  )
		                                  | std::views::join
		                                  | std::ranges::to<std::string>();

		const std::string dict_html = mdict->lookup(lookup_string);
		if (dict_html.empty()) {
			loadDictEntry(iter1, iter2 + 1, max_length, length);
			return;
		}
		const std::string strip_dict_html   = trim_copy(dict_html);
		const std::string dict_html_wrapped = "<html><head><style>" + css_data + "</style></head><body>" +
		                                      strip_dict_html + "</body></html>";

		const std::u16string dict_html_16 = utf8::utf8to16(dict_html_wrapped);
		const std::wstring   dict_html_w(dict_html_16.begin(), dict_html_16.end());

		HRESULT nav_err = webview->NavigateToString(dict_html_w.c_str());
		log(nav_err, "ICoreWebView2.NavigateToString", ERR_LEVEL::FATAL);

		EventRegistrationToken& tokenRef = dict_init_nav_tokens[iter1];
		nav_err                          = webview->add_NavigationCompleted(
			Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
				[this, iter1, iter2, max_length, length, lookup_string, strip_dict_html, &tokenRef](
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
							[this, iter1, iter2, max_length, length, lookup_string, strip_dict_html, &tokenRef, sender](
						HRESULT errorCode,
						LPCWSTR resultObjectAsJson
					) -> HRESULT {
								spdlog::info("Loaded dict data for {}.{} len={}", iter1, iter2, length);
								log(errorCode, "ICoreWebView2ExecuteScriptCompletedHandler.Invoke", ERR_LEVEL::WARN);

								if (SUCCEEDED(errorCode)) {
									const int webview_width = _wtoi(resultObjectAsJson) + scroll_bar_width;

									if (const auto iter = dictionary_data.find(results[iter1].results[iter2].text);
										iter != dictionary_data.end()) {
										iter->second.entries.emplace_back(
											strip_dict_html,
											webview_width,
											lookup_string
										);
									} else {
										std::vector<DictionaryEntry> new_vec{};
										new_vec.emplace_back(strip_dict_html, webview_width, lookup_string);
										dictionary_data.insert(
											{
												results[iter1].results[iter2].text,
												{new_vec}
											}
										);
									}


									const HRESULT rem_err = sender->remove_NavigationCompleted(tokenRef);
									log(rem_err, "ICoreWebView2.remove_NavigationCompleted", ERR_LEVEL::WARN);

									loadDictEntry(iter1, iter2 + 1, max_length, length);
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

	std::vector<OCRBlock> TooltipWnd::processOCRResults(
		const std::vector<OCRResult>& res,
		const cv::Point&              topleft
	) {
		std::vector<OCRBlock> res_packed{};
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
				for (int i = 0; i < res_packed.size(); i++) {
					if (res_packed[i].horizontal & intersects(res_packed[i].poly, moved_rect)) {
						intersections.push_back(i);
					}
				}
				if (intersections.empty()) {
					std::vector<cv::Point> poly{};
					poly.append_range(moved_rect);
					res_packed.emplace_back(split_line, true, poly);
				} else {
					std::ranges::reverse(intersections);
					OCRBlock& first = res_packed[intersections[0]];
					first.results.append_range(split_line);
					first.poly = union_(first.poly, moved_rect);
					for (int i = 1; i < intersections.size(); i++) {
						OCRBlock curr = res_packed[intersections[i]];
						first.results.append_range(curr.results);
						first.poly = union_(first.poly, curr.poly);
						res_packed.erase(res_packed.begin() + intersections[i]);
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
				for (int i = 0; i < res_packed.size(); i++) {
					if (!res_packed[i].horizontal & intersects(res_packed[i].poly, moved_rect)) {
						intersections.push_back(i);
					}
				}
				if (intersections.empty()) {
					std::vector<cv::Point> poly{};
					poly.append_range(moved_rect);
					res_packed.emplace_back(split_line, true, poly);
				} else {
					std::ranges::reverse(intersections);
					OCRBlock& first = res_packed[intersections[0]];
					first.results.append_range(split_line);
					first.poly = union_(first.poly, moved_rect);
					for (int i = 1; i < intersections.size(); i++) {
						OCRBlock curr = res_packed[intersections[i]];
						first.results.append_range(curr.results);
						first.poly = union_(first.poly, curr.poly);
						res_packed.erase(res_packed.begin() + intersections[i]);
					}
				}
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
		wnd->results = processOCRResults(res, cv::Point{rect.x, rect.y});

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
		if (GetAsyncKeyState(VK_SHIFT) & (1 << 15)) {
			POINT win_mouse_pos;
			GetCursorPos(&win_mouse_pos);

			const cv::Point mouse_pos{win_mouse_pos.x, win_mouse_pos.y};
			if (rect.contains(mouse_pos)) {
				const auto intersect_iter = std::ranges::find_if(
					results,
					[&mouse_pos](const auto res) {
						// returns positive (inside), negative (outside), or zero (on an edge) value
						return cv::pointPolygonTest(res.poly, mouse_pos, false) > 0;
					}
				);
				if (intersect_iter != results.end()) {
					if (is_hovering && cv::pointPolygonTest(prev_hover_rect, mouse_pos, false) > 0) {
						// caching previous rect in case (optimisation)
					} else {
						const auto word_iter = std::ranges::find_if(
							intersect_iter->results,
							[&mouse_pos](const auto res) {
								// returns positive (inside), negative (outside), or zero (on an edge) value
								return cv::pointPolygonTest(res.rect, mouse_pos, false) > 0;
							}
						);
						if (word_iter != intersect_iter->results.end()) {
							is_hovering     = true;
							prev_hover_rect = word_iter->rect;
							hover_text      = word_iter->text;
							refreshWindow();
						} else {
							is_hovering = false;
						}
					}
				} else {
					// mouse isn't in any of the OCR areas
					is_hovering = false;
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
