#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wil/com.h>
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

#include <opencv2/core/mat.hpp>
#include <mdict.h>
#include <WebView2.h>

#include "common.h"
#include "implwebview2.h"

namespace ocr {
	struct OCRResult;

	class TooltipWnd {
	private:
		static inline const std::string className     = "TooltipWnd";
		static inline bool              isInitialised = false;
		static constexpr int            min_width     = 128;
		static constexpr int            min_height    = 256;

		int                           width, height;
		bool                          is_hovering;
		Poly2I                        prev_hover_rect;
		POINT                         prev_hover_point;
		std::string                   hover_text;
		std::string                   dictionary_html;
		std::unique_ptr<mdict::Mdict> mdict;

		ID2D1Factory*          d2d1_factory             = nullptr;
		ID2D1HwndRenderTarget* render_target            = nullptr;
		ID2D1SolidColorBrush*  brush                    = nullptr;
		IDWriteFactory*        direct_write_factory     = nullptr;
		IDWriteTextFormat*     direct_write_text_format = nullptr;

		std::unique_ptr<WebView2::Impl>       wv_init;
		wil::com_ptr<ICoreWebView2Controller> wv_controller;
		wil::com_ptr<ICoreWebView2>           webview;
		bool                                  initedWebView2{false};


		bool initDirectWrite();

		void cleanupDirectWrite();

		std::tuple<float, float> getTextSize(
			const std::u16string& w_hover_text,
			float                 p_width  = FLT_MAX,
			float                 p_height = FLT_MAX
		) const;

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		HWND                         hwnd;
		bool                         is_running = true;
		std::vector<OCRResultPacked> results;

		TooltipWnd() = default;

		~TooltipWnd();

		static std::vector<OCRResultPacked> processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			const bool                    separate_characters
		);

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			const std::string&            dict_path
		);

		bool initWebView2();

		void updateLoop();
	};
}
