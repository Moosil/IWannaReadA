#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

#include <opencv2/core/mat.hpp>

#include "common.h"

namespace ocr {
	struct OCRResult;

	class TooltipWnd {
	private:
		static inline const std::string className = "TooltipWnd";
		static inline bool isInitialised = false;

		LONG width, height;
		bool is_hovering;
		Poly2I prev_hover_rect;
		POINT prev_hover_point;
		std::string hover_text;

		ID2D1Factory* d2d1_factory = nullptr;
		ID2D1HwndRenderTarget* render_target = nullptr;
		ID2D1SolidColorBrush* brush = nullptr;
		IDWriteFactory* direct_write_factory = nullptr;
		IDWriteTextFormat* direct_write_text_format = nullptr;


		bool initDirectWrite();

		void cleanupDirectWrite();

		std::tuple<float, float> getTextSize(const std::wstring& w_hover_text) const;

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);


	public:
		HWND hwnd;
		bool is_running = true;
		std::vector<OCRResultPacked> results;

		TooltipWnd() = default;
		static std::unique_ptr<TooltipWnd> initTooltip(const std::vector<OCRResult>& res, const cv::Point& topleft);

		void updateLoop();
	};
}