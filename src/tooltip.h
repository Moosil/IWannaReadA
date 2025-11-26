#pragma once

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wil/com.h>
#pragma comment(lib,"d2d1")
#pragma comment(lib,"dwrite")

#include <map>
#include <mdict.h>
#include <WebView2.h>

#include "common.h"
#include "implwebview2.h"

namespace ocr {
	struct DictionaryData {
		std::wstring entry;
		int width;
	};

	class TooltipWnd {
	private:
		static inline const std::string className        = "TooltipWnd";
		static inline bool              isInitialised    = false;
		static constexpr int            title_bar_height = 32;
		static constexpr int            min_width        = 128;
		static constexpr int            min_height       = 256;
		static constexpr int scroll_bar_width = 16;

		int                                width{}, height{};
		bool                               is_hovering{};
		Poly2I                             prev_hover_rect;
		cv::Rect                           rect;
		std::string                        hover_text;

		std::vector<OCRResultPacked> results;
		std::unique_ptr<mdict::Mdict>      mdict;
		std::string                        css_data;
		std::map<std::string, DictionaryData> dictionary_data;
		std::vector<EventRegistrationToken> dict_init_nav_tokens;
		bool inited_dictionary{false};

		ID2D1Factory*          d2d1_factory             = nullptr;
		ID2D1HwndRenderTarget* render_target            = nullptr;
		ID2D1SolidColorBrush*  brush                    = nullptr;
		IDWriteFactory*        direct_write_factory     = nullptr;
		IDWriteTextFormat*     direct_write_text_format = nullptr;

		std::unique_ptr<WebView2::Impl>       wv_init;
		wil::com_ptr<ICoreWebView2Controller> wv_controller;
		wil::com_ptr<ICoreWebView2>           webview;
		bool                                  inited_web_view2{false};


		bool initDirectWrite();

		void initWebView2();

		void cleanupDirectWrite() const;

		void loadDictionary(const std::string& dict_string_path);

		void updateWindowSize() const;

		void startDictLoading();

		void loadDictEntry(int iter);

		[[nodiscard]] std::tuple<float, float> getTextSize(
			const std::u16string& w_hover_text,
			float                 p_width  = FLT_MAX,
			float                 p_height = FLT_MAX
		) const;

		static std::vector<OCRResultPacked> processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			bool                          separate_characters
		);

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		HWND                         hwnd{};
		bool                         is_running = true;

		TooltipWnd() = default;

		~TooltipWnd();

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>& res,
			const cv::Rect&               rect,
			const std::string&            dict_folder_path
		);

		void updateLoop();
	};
}
