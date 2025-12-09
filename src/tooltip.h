#pragma once

#include <chrono>
#include <Windows.h>
#include <wil/com.h>

#include <mdict.h>
#include <stack>
#include <unordered_map>
#include <WebView2.h>

#include "common.h"

struct ID2D1Factory;
struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct IDWriteFactory;
struct IDWriteTextFormat;

struct ICoreWebView2Controller;
struct ICoreWebView2;
struct EventRegistrationToken;
struct ICoreWebView2WebMessageReceivedEventHandler;

namespace WebView2 {
	class Impl;
}

namespace ocr {
	struct DictionaryEntry {
		std::string entry;
		std::string entry_html;
	};

	struct DictionaryData {
		std::vector<DictionaryEntry> entries;
		int                          width{-1};
	};

	struct OCRBlock {
		std::vector<OCRResultPacked> results;
		bool                         horizontal;
		std::vector<cv::Point>       poly;
	};

	class TooltipWnd {
	private:
		static constexpr auto boilerplate_html = LR"(<html><body><div id="root"></div><script>
document.addEventListener("contextmenu", e => {
    const host = e.target.getRootNode().host;
    if (host) {
        window.chrome.webview.postMessage({
            key: "contextmenu",
            x: e.screenX,
            y: e.screenY,
            word: host.id
        });
        e.preventDefault();
    }
});

document.addEventListener("mousedown", e => {
    const host = e.target.getRootNode().host;
    if (host) {
        window.chrome.webview.postMessage({
            key: "mousedown",
            x: e.screenX,
            y: e.screenY
        });
    }
});</script></body></html>)";

		static inline const std::string className        = "TooltipWnd";
		static inline bool              isInitialised    = false;
		static constexpr int            title_bar_height = 32;
		static constexpr int            min_width        = 256;
		static constexpr int            min_height       = 256;
		static constexpr int            scroll_bar_width = 16;

		int                                                             width{}, height{};
		bool                                                            is_hovering{};
		cv::Rect                                                        rect;
		std::ranges::borrowed_iterator_t<std::vector<OCRBlock>&>        hover_block;
		std::ranges::borrowed_iterator_t<std::vector<OCRResultPacked>&> hover_word;
		bool                                                            need_refresh{false};
		std::optional<std::chrono::steady_clock::time_point>            start;

		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::unique_ptr<mdict::Mdict>                   mdict;
		std::string                                     css_data;
		std::unordered_map<std::string, DictionaryData> dictionary_data;
		int                                             max_webpage_width{scroll_bar_width};

		wil::com_ptr<ID2D1Factory>          d2d1_factory             = nullptr;
		wil::com_ptr<ID2D1HwndRenderTarget> render_target            = nullptr;
		wil::com_ptr<ID2D1SolidColorBrush>  brush                    = nullptr;
		wil::com_ptr<IDWriteFactory>        direct_write_factory     = nullptr;
		wil::com_ptr<IDWriteTextFormat>     direct_write_text_format = nullptr;

		std::unique_ptr<WebView2::Impl>       wv_init;
		wil::com_ptr<ICoreWebView2Controller> wv_controller;
		wil::com_ptr<ICoreWebView2>           webview;
		bool                                  inited_web_view2{false};


		void initDirectWrite();

		void initWebView2();

		void cleanupDirectWrite();

		void initDictionary(const std::string& dict_string_path);

		void updateWindowSize() const;

		void updateWindowPosition() const;

		void refreshWindow();

		void onWebsiteChanged();

		HRESULT onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

		void createContextMenu(int x, int y, const std::string& phrase) const;

		void initCurrDictHTML();

		static void processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			std::vector<OCRBlock>&        out
		);

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		HWND hwnd{};
		bool is_running = true;

		TooltipWnd() = default;

		~TooltipWnd();

		HRESULT onNavigationComplete();

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>& res,
			const cv::Rect&               rect,
			const std::string&            dict_folder_path
		);

		void updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

		void updateLoop();
	};
}
