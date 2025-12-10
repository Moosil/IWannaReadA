#pragma once

#include <mdict.h>
#include <stack>
#include <unordered_map>

#include <Windows.h>
#include <wrl/client.h>

#include "common.h"

struct ICoreWebView2Controller;
struct ICoreWebView2;
struct EventRegistrationToken;
struct ICoreWebView2WebMessageReceivedEventHandler;
struct ICoreWebView2WebMessageReceivedEventArgs;

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
		static constexpr int            min_width        = 256;
		static constexpr int            min_height       = 256;
		static constexpr int            scroll_bar_width = 16;
		static constexpr char html_skeleton[] = ""
		"<html><body>"
			"<div id=\"0\"><template shadowrootmode=\"open\"><style>{}</style><div></div></template></div>"
			"<div id=\"1\"><template shadowrootmode=\"open\"><style>{}</style><div></div></template></div>"
			"<div id=\"2\"><template shadowrootmode=\"open\"><style>{}</style><div></div></template></div>"
			"<div id=\"3\"><template shadowrootmode=\"open\"><style>{}</style><div></div></template></div>"
			"<div id=\"4\"><template shadowrootmode=\"open\"><style>{}</style><div></div></template></div>"
		"</body></html>";
		static constexpr char fill_webpage_script[] = ""
		"(() => {{"
			"const host = document.getElementById(\"{}\");"
			"const shadow = host.shadowRoot;"
			"const div = shadow.lastChild;"
			"div.id = `{}`;"
			"div.innerHTML = `{}`;"
		"}})();";
		static constexpr wchar_t reset_webpage_script[] = L""
		"for (const host of document.body.children) {"
			"const div = host.shadowRoot.lastChild;"
			"div.id=\"\";"
			"div.innerHTML=\"\";"
		"};";
		static constexpr wchar_t get_width_script[] = L""
		"(() => {{"
			"const html = document.documentElement;"
			"const body = document.body;"
			"return Math.max(html.scrollWidth, body.scrollWidth);"
		"}})();";
		static constexpr wchar_t add_context_menu_script[] = L""
			"for (const host of document.body.children) {"
				"const shadow = host.shadowRoot;"
				"shadow.addEventListener(\"contextmenu\", function (event) {"
					"let jsonObject = {"
						"key: \"contextmenu\","
						"x: event.screenX,"
						"y: event.screenY,"
						"word: host.id"
					"};"
					"window.chrome.webview.postMessage(jsonObject);"
					"event.preventDefault();"
				"});"
				"shadow.addEventListener(\"mousedown\", function (event) {"
					"let jsonObject = {"
						"key: 'mousedown',"
						"x: event.screenX,"
						"y: event.screenY"
					"};"
					"window.chrome.webview.postMessage(jsonObject);"
				"});"
			"}"
		")";
		
		int                                                             width{}, height{};
		bool                                                            is_hovering{};
		cv::Rect                                                        rect;
		std::ranges::borrowed_iterator_t<std::vector<OCRBlock>&>        hover_block;
		std::ranges::borrowed_iterator_t<std::vector<OCRResultPacked>&> hover_word;
		bool                                                            need_refresh{false};

		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::unique_ptr<mdict::Mdict>                   mdict;
		std::string                                     css_data;
		std::unordered_map<std::string, DictionaryData> dictionary_data;
		int                                             max_webpage_width{scroll_bar_width};

		std::unique_ptr<WebView2::Impl>                 wv_init;
		Microsoft::WRL::ComPtr<ICoreWebView2Controller> wv_controller;
		Microsoft::WRL::ComPtr<ICoreWebView2>           webview;
		bool                                            inited_web_view2{false};

		void initWebView2();

		void initDictionary(const std::string& dict_string_path);

		void initCurrDictHTML();

		static void processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			std::vector<OCRBlock>&        out
		);

		void updateWindowSize() const;

		void updateWindowPosition() const;

		void refreshWindow();

		void onNavigationComplete();

		HRESULT onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

		void createContextMenu(int x, int y, const std::string& phrase) const;

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		HWND hwnd{};
		bool is_running = true;

		TooltipWnd() = default;

		~TooltipWnd();

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>& res,
			const cv::Rect&               rect,
			const std::string&            dict_folder_path
		);

		void updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

		void updateLoop();
	};
}
