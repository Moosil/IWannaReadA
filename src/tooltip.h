#pragma once

#include <unordered_map>
#include <Windows.h>
#include <wrl/client.h>

#include "anki_connect.h"
#include "common.h"
#include "dict_parser.h"

struct ICoreWebView2Controller;
struct ICoreWebView2;
struct EventRegistrationToken;
struct ICoreWebView2WebMessageReceivedEventHandler;
struct ICoreWebView2WebMessageReceivedEventArgs;

namespace WebView2 {
	class Impl;
}

namespace iwra {
	struct DictionaryData {
		std::vector<DictParser::entry> entries{};
		std::string phrase;
		int height = -1;
	};

	struct OCRBlock {
		std::vector<OCRResultPacked> results;
		bool                         horizontal;
		std::vector<cv::Point>       poly;
	};

	class TooltipWnd {
	private:
		static inline const std::string className        = "TooltipWnd";
		static inline bool              isInitialised    = false;
		static constexpr int            min_width        = 384;
		static constexpr int            min_height       = 256;
		static constexpr int            max_height       = 1024;
		static constexpr int            scroll_bar_width = 16;
		static constexpr wchar_t get_width_script[] = L""
		"(() => {{"
			"const body = document.body;"
			"const html = document.documentElement;"

			"return Math.max(body.scrollWidth, body.offsetWidth,"
							"html.scrollWidth, html.offsetWidth, html.clientWidth);"
		"}})();";
		static constexpr wchar_t get_height_script[] = L""
		"(() => {{"
			"const body = document.body;"
			"const html = document.documentElement;"

			"return Math.max(body.scrollHeight, body.offsetHeight,"
							"html.scrollHeight, html.offsetHeight, html.clientHeight);"
		"}})();";

		int                                                             width{}, height{};
		bool                                                            is_hovering{};
		cv::Rect                                                        rect;
		OCRBlock*      hover_block;
		OCRResultPacked* hover_word;
		bool                                                            need_refresh{false};

		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::shared_ptr<DictParser> parser;
		std::unordered_map<std::string, DictionaryData> dictionary_data;
		int                                             max_webpage_width{scroll_bar_width};

		std::unique_ptr<WebView2::Impl>                 wv_init;
		Microsoft::WRL::ComPtr<ICoreWebView2Controller> wv_controller;
		Microsoft::WRL::ComPtr<ICoreWebView2>           webview;
		bool                                            inited_web_view2{false};
		std::string webpage_html;

		std::shared_ptr<Anki::Interface> anki;

		TooltipWnd(
			const std::vector<OCRResult>&           res,
			const cv::Rect&                         rect,
			const std::filesystem::path&            webpage_path,
			const std::shared_ptr<DictParser>&      parser,
			const std::shared_ptr<Anki::Interface>& anki
		);

		void initWebView2();

		void initCurrDict();

		static void processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			std::vector<OCRBlock>&        out
		);

		static std::vector<OCRResultPacked> ocrSplitText(const Poly2I& rect, const Text& text, bool horizontal);

		void updateWindowSize() const;

		void updateWindowPosition() const;

		void refreshWindow();

		void refreshHovering();

		HRESULT onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

		void onNavigationComplete();

		void addAnkiCard(const std::string& character, const std::string& phrase, const std::string& pinyin, const std::string& sentence, const std::string& definition) const;

		void createContextMenu(int x, int y, const std::string& character, const std::string& phrase, const std::string& pinyin, const std::string&
		                       sentence,
		                       const std::string& definition
		) const;

		static std::string getSentence(OCRBlock* hover_block);

		static std::string getPhrase(const OCRResultPacked* hover_word, const OCRBlock* hover_block);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		HWND hwnd{};
		bool is_running = true;

		TooltipWnd() = delete;

		~TooltipWnd();

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>&           res,
			const cv::Rect&                         rect,
			const std::filesystem::path&            webpage_path,
			const std::shared_ptr<DictParser>&      parser,
			const std::shared_ptr<Anki::Interface>& anki
		);

		void updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

		void updateLoop();
	};
}
