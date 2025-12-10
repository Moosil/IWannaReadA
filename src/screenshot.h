#pragma once

#include <Windows.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace ocr {
	class ScreenshotWnd {
	private:
		static inline const std::string className     = "ScreenshotWnd";
		static inline bool              isInitialised = false;

		bool       is_dragging = false;
		POINT      start{0, 0};
		POINT      end{0, 0};
		HBITMAP    desktop;
		cv::Mat*   screenshot;
		cv::Rect* rect;

		HDC     darkenDC;
		HBITMAP darkenBitmap;
		LONG    width, height;

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		HWND hwnd;
		bool is_running = true;

		ScreenshotWnd() = default;

		static std::unique_ptr<ScreenshotWnd> startScreenShot(cv::Mat* ss, cv::Rect* rect);

		static HBITMAP captureEntireScreen();

		static HBITMAP captureScreenRegion(cv::Rect rect);

		static cv::Mat hBitmap2cvMat(HBITMAP h_bitmap);

		void update() const;
	};
}
