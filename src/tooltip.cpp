#include "tooltip.h"

#include <ranges>

#include <gdiplus.h>
#include <dcomp.h>
#include <d2d1helper.h>
#include <wrl.h>
#include <dwrite.h>
#pragma comment(lib,"gdiplus")
#pragma comment(lib,"dxgi")
#pragma comment(lib,"dcomp")
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

#include <filesystem>
#include <Windowsx.h>
#include <spdlog/spdlog.h>
#include <opencv2/imgproc.hpp>
#include <utf8/cpp20.h>
#include <clip.h>
#include <nlohmann/json.hpp>
#include <litehtml.h>

#include "impl_document_container.h"
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
		const int screen_width = getScreenSize().first;
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
		if (!is_hovering) {
			return;
		}
		if (!start.has_value()) {
			start = std::chrono::steady_clock::now();
		}

		height = min_height;
		width = min_width;

		const auto it = dictionary_data.find(hover_word->text);
		if (it == dictionary_data.end()) {
			initCurrDictHTML();
			need_refresh = true;
		} else {
			std::string total_website;
			auto&       [entries, webpage_width] = it->second;
			if (webpage_width == -1) {
				webpage_width = max_webpage_width;
			}

			for (const auto& [entry, html] : entries) {
				// Shadow DOM template to isolate duplicated HTML ids
				total_website += fmt::format(
					R"(<div id="{}">{}</div>)",
					entry,
					html
				);
				width = std::max(width, webpage_width);
			}

			const std::string wrapped_html = fmt::format(
				R"(<html><head><style>{}</style></head><body>{}</body></html>)",
				css_data,
				total_website
			);

			html_renderer = litehtml::document::createFromString(wrapped_html.c_str(), html_doc_impl.get());

			onWebsiteChanged();
		}
	}

	void TooltipWnd::onWebsiteChanged() {
		const auto dict_data_it = dictionary_data.find(hover_word->text);
		if (dict_data_it != dictionary_data.end()) {
			const int best_width = html_renderer->render(width);
			dict_data_it->second.width = best_width;
			max_webpage_width = std::max(max_webpage_width, best_width + scroll_bar_width);
		}

		updateWindowSize();

		updateWindowPosition();
		if (start.has_value()) {
			const auto end = std::chrono::steady_clock::now();
			const auto duration = std::chrono::duration<double, std::milli>(end - start.value());
			spdlog::info("website navigate duration: {}ms", duration.count());

			start.reset();
		}
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
		const auto        result_it = hover_block;
		std::string       lookup_string;
		const std::string first_char = hover_word->text;
		for (auto word_it = hover_word; word_it != result_it->results.end(); ++word_it) {
			lookup_string += word_it->text;

			const std::string dict_html = mdict->lookup(lookup_string);
			if (dict_html.empty()) {
				continue;
			}
			const std::string strip_dict_html = trim_copy(dict_html);
			const std::string static_lookup_string(lookup_string);

			dictionary_data[first_char].entries.emplace_back(static_lookup_string, strip_dict_html);
		}
		std::ranges::reverse(dictionary_data[first_char].entries);
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

		wnd->initDirectWrite();

		wnd->html_doc_impl = std::make_unique<HTMLDocument>(HTMLDocument{*wnd, wnd->direct_write_factory, wnd->render_target});


		wnd->initDictionary(dict_folder_path);

		return wnd;
	}

	void TooltipWnd::updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect) {
		rect = new_rect;
		is_hovering = false;

		ShowWindow(hwnd, SW_HIDE);
		UpdateWindow(hwnd);

		processOCRResults(new_res, cv::Point{rect.x, rect.y}, results);
	}

	void TooltipWnd::initDirectWrite() {
		HRESULT err = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d1_factory.GetAddressOf());
		log(err, "D2D1CreateFactory", ERR_LEVEL::FATAL);

		err = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(direct_write_factory.GetAddressOf())
		);
		log(err, "ID2D1Factory::DWriteCreateFactory", ERR_LEVEL::FATAL);

		const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		);
		const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
			hwnd,
			D2D1::SizeU(static_cast<unsigned int>(width), static_cast<unsigned int>(height))
		);

		err = d2d1_factory->CreateHwndRenderTarget(rtProps, hwndProps, render_target.GetAddressOf());
		log(err, "ID2D1Factory::CreateHwndRenderTarget", ERR_LEVEL::FATAL);
	}

	void TooltipWnd::cleanupDirectWrite() {
		render_target.Reset();
		direct_write_factory.Reset();
		d2d1_factory.Reset();
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
							is_hovering  = true;
							hover_word   = word_iter;
							hover_block  = intersect_iter;
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
				ShowWindow(hwnd, SW_HIDE);
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

	std::pair<int, int> TooltipWnd::getExtent() const {
		return {width, height};
	}

	LRESULT TooltipWnd::wndProc(const UINT msg, const WPARAM wparam, const LPARAM lparam) {
		switch (msg) {
			case WM_CREATE: {
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

				if (html_renderer) {
					html_renderer->render(width);
				}
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

					if (html_renderer) {
						const litehtml::position pos = {0, 0, width, height};
							html_renderer->draw(reinterpret_cast<litehtml::uint_ptr>(render_target.Get()), 0, 0, &pos);
					}

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
