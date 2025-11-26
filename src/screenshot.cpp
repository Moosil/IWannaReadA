//
// Created by rowan on 20/11/2025.
//

#include "screenshot.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>
#include <spdlog/spdlog.h>
#include <Windowsx.h>

#include "util.h"


namespace ocr {
	std::unique_ptr<ScreenshotWnd> ScreenshotWnd::startScreenShot(cv::Mat* ss, cv::Rect* rect) {
		auto wnd = std::make_unique<ScreenshotWnd>();
		if (!isInitialised) {
			WNDCLASS wc{};
			wc.lpfnWndProc = &wndProcSetup;
			wc.hInstance = GetModuleHandle(nullptr);
			wc.lpszClassName = className.c_str();
			wc.hbrBackground = nullptr;
			if (!RegisterClassA(&wc)) {
				spdlog::error("registering window class failed");
				return nullptr;
			}
			isInitialised = true;
		}

		constexpr int style           = WS_POPUP | WS_VISIBLE;
		constexpr int extended_styles = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

		const auto [width, height] = getScreenSize();
		wnd->width = static_cast<long>(width);
		wnd->height = static_cast<long>(height);

		wnd->screenshot = ss;
		wnd->rect = rect;

		wnd->hwnd = CreateWindowEx(
			extended_styles,
			className.c_str(),
			"ss",
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

		return wnd;
	}

	HBITMAP ScreenshotWnd::captureEntireScreen() {
		const auto [width, height] = getScreenSize();

		return captureScreenRegion(cv::Rect(0, 0, static_cast<int>(width), static_cast<int>((height))));
	}

	HBITMAP ScreenshotWnd::captureScreenRegion(const cv::Rect rect) {
		HDC h_source = GetDC(nullptr);
		HDC h_memory = CreateCompatibleDC(h_source);

		HBITMAP h_bitmap = CreateCompatibleBitmap(h_source, rect.width, rect.height);
		SelectObject(h_memory, h_bitmap);

		BitBlt(h_memory, 0, 0, rect.width, rect.height, h_source, rect.x, rect.y, SRCCOPY);

		DeleteDC(h_memory);
		ReleaseDC(nullptr, h_source);

		return h_bitmap;
	}

	cv::Mat ScreenshotWnd::hBitmap2cvMat(HBITMAP h_bitmap) {
		BITMAP bitmap;
		GetObject(h_bitmap, sizeof(bitmap), &bitmap);

		cv::Mat mat(bitmap.bmHeight, bitmap.bmWidth, CV_8UC4);
		GetBitmapBits(h_bitmap, bitmap.bmHeight * bitmap.bmWidthBytes, mat.data);

		// Convert BGRA → BGR
		cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);

		DeleteObject(h_bitmap);
		return mat.clone();
	}

	void ScreenshotWnd::update() const {
		MSG msg;
		while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	LRESULT CALLBACK ScreenshotWnd::wndProc(const UINT msg, const WPARAM wparam, const LPARAM lparam) {
		switch (msg) {
			case WM_CREATE: {
				desktop = captureEntireScreen();

				HDC hdc = GetDC(hwnd);
				darkenDC = CreateCompatibleDC(hdc);
				darkenBitmap = CreateCompatibleBitmap(hdc, 1, 1);
				SelectObject(darkenDC, darkenBitmap);

				constexpr RECT black_pixel_rect = {0,0,1,1};
				FillRect(darkenDC, &black_pixel_rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
				SetCapture(hwnd);
				break;
			}

			case WM_LBUTTONDOWN: {
				is_dragging = true;
				start = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
				break;
			}

			case WM_MOUSEMOVE: {
				if (is_dragging) {
					end = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
					InvalidateRect(hwnd, nullptr, FALSE);
				}
				break;
			}

			case WM_LBUTTONUP: {
				if (is_dragging) {
					is_dragging = false;
					is_running = false;
					ReleaseCapture();

					end = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

					if (start.x != -1 && start.y != -1) {
						const auto [left, right] = std::minmax(start.x, end.x);
						const auto [top, bottom] = std::minmax(start.y, end.y);
						const auto r_width = right - left;
						const auto r_height = bottom - top;
						const auto bitmap = captureScreenRegion(cv::Rect(left, top, r_width, r_height));
						*screenshot = hBitmap2cvMat(bitmap);
						*rect = {left, top, r_width, r_height};
					}
					DestroyWindow(hwnd);
				}
				break;
			}

			case WM_KEYDOWN: {
				if (wparam == VK_ESCAPE) {
					spdlog::info("aborting screenshot");
					start = { -1, -1 };
					end = { -1, -1 };
					is_dragging = false;
					is_running = false;
					ReleaseCapture();
					*rect = cv::Rect(-1, -1, -1, -1);
					DestroyWindow(hwnd);
				}
				break;
			}

			case WM_PAINT: {
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);

				if (desktop) {
					HDC memory = CreateCompatibleDC(hdc);
					HBITMAP memory_bitmap = CreateCompatibleBitmap(hdc, width, height);
					HGDIOBJ old_memory_bitmap = SelectObject(memory, memory_bitmap);
					HDC desktop_dc = CreateCompatibleDC(hdc);
					HGDIOBJ old_desktop_bitmap = SelectObject(desktop_dc, desktop);

					BitBlt(memory, 0, 0, width, height, desktop_dc, 0, 0, SRCCOPY);

					// Clean up the desktop DC
					SelectObject(desktop_dc, old_desktop_bitmap);
					DeleteDC(desktop_dc);

					// Darken outside rectangle
					constexpr BLENDFUNCTION bf = {
						AC_SRC_OVER,
						0,
						128,
						0
					};

					const auto [left, right] = std::minmax(start.x, end.x);
					const auto [top, bottom] = std::minmax(start.y, end.y);

					// top
					if (top > 0)
						AlphaBlend(memory, 0, 0, width, top, darkenDC, 0,0,1,1, bf);

					// bottom
					if (bottom < height)
						AlphaBlend(memory, 0, bottom, width, height - bottom, darkenDC, 0,0,1,1, bf);

					// left
					if (left > 0)
						AlphaBlend(memory, 0, top, left, bottom - top, darkenDC, 0,0,1,1, bf);

					// right
					if (right < width)
						AlphaBlend(memory, right, top, width - right, bottom - top, darkenDC, 0,0,1,1, bf);


					BitBlt(hdc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
					SelectObject(memory, old_memory_bitmap);
					DeleteObject(old_memory_bitmap);
					DeleteDC(memory);
				}

				EndPaint(hwnd, &ps);
				return 0;
			}

			case WM_DESTROY: {
				if (desktop) {
					DeleteObject(desktop);
					DeleteObject(darkenBitmap);

					DeleteDC(darkenDC);
				}
				ReleaseCapture();
				break;
			}
			default: {};
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	LRESULT CALLBACK ScreenshotWnd::wndProcSetup(HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam) {
		ScreenshotWnd* self;

		if (msg == WM_NCCREATE) {
			const auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
			self = static_cast<ScreenshotWnd*>(cs->lpCreateParams);
			self->hwnd = hwnd;
			SetLastError(0);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
			if (GetLastError() != 0) {
				return false;
			}
		} else {
			self = reinterpret_cast<ScreenshotWnd*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		if (self) {
			return self->wndProc(msg, wparam, lparam);
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
}
