#pragma once

#include <unordered_map>
#include <Windows.h>
#include <wrl/client.h>

#include "anki_connect.h"
#include "common.h"
#include "dict_parser.h"
#include "implwebview2.h"

namespace iwra {
	struct DictionaryData {
		std::vector<DictParser::entry> entries{};
		std::string                    phrase;
	};

	struct OCRBlock {
		std::vector<OCRResultPacked> results;
		std::vector<cv::Point>       poly;
		bool                         horizontal;
	};

	class TooltipWnd {
	private:
		static inline const std::string className          = "TooltipWnd";
		static inline bool              isInitialised      = false;
		static constexpr int            width          = 384;
		static constexpr int            height         = 256;
		static constexpr int            scroll_bar_width   = 16;
		static constexpr wchar_t        get_width_script[] = L""
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

		HWND             hwnd{};
		cv::Rect         rect;
		OCRBlock*        current_block{nullptr};
		OCRResultPacked* current_word{nullptr};
		std::string current_phrase{};

		bool             need_refresh{false};
		bool             is_hovering{false};
		bool             inited_web_view2{false};
		int              max_webpage_width{scroll_bar_width};
		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::shared_ptr<DictParser>                     parser;
		std::unordered_map<std::string, DictionaryData> dictionary_data;

		std::unique_ptr<WebView2::Impl>                 wv_init;
		Microsoft::WRL::ComPtr<ICoreWebView2Controller> wv_controller;
		Microsoft::WRL::ComPtr<ICoreWebView2>           webview;
		std::string                                     webpage_html;

		std::shared_ptr<Anki::Interface> anki;

		void initWebView2();

		bool initCurrDict();

		static void processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			std::vector<OCRBlock>&        out
		);

		static std::vector<OCRResultPacked> ocrSplitText(const Poly2I& rect, const Text& text, bool horizontal);

		void updateWindowPosition() const;

		const DictionaryData* getDictDataOrInit(const std::string& key);

		void updateWindowEntry(const DictionaryData* dict_data, const std::string& phrase, const std::string& sentence) const;

		void refreshWindow();

		void refreshHovering();

		HRESULT onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

		void onNavigationComplete();

		void addAnkiCard(
			const std::string& character,
			const std::string& phrase,
			const std::string& pinyin,
			const std::string& sentence,
			const std::string& definition
		) const;

		void createContextMenu(
			int                x,
			int                y,
			const std::string& character,
			const std::string& phrase,
			const std::string& pinyin,
			const std::string&
			sentence,
			const std::string& definition
		) const;

		static std::string getSentence(OCRBlock* hover_block);

		static std::string getPhrase(const OCRResultPacked* hover_word, const OCRBlock* hover_block);

		LRESULT CALLBACK wndProc(UINT msg, WPARAM wparam, LPARAM lparam);

		static LRESULT CALLBACK wndProcSetup(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	public:
		TooltipWnd() = delete;

		TooltipWnd(
			const std::vector<OCRResult>&           res,
			const cv::Rect&                         rect,
			const std::filesystem::path&            webpage_path,
			const std::shared_ptr<DictParser>&      parser,
			const std::shared_ptr<Anki::Interface>& anki
		);

		~TooltipWnd() = default;

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>&           res,
			const cv::Rect&                         rect,
			const std::filesystem::path&            webpage_path,
			const std::shared_ptr<DictParser>&      parser,
			const std::shared_ptr<Anki::Interface>& anki
		);

		void updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

		void updateLoop();

		HWND getHwnd() const;
	};
}
