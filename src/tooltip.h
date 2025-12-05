#pragma once

#include <Windows.h>

#include <stack>
#include <unordered_map>
#include <chrono>

#include <mdict.h>
#include <wrl/client.h>

#include "common.h"
#include "impl_document_container.h"

namespace ocr {
}

struct ID2D1Factory;
struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct IDWriteFactory;
struct IDWriteTextFormat;

struct ICoreWebView2Controller;
struct ICoreWebView2;
struct EventRegistrationToken;
struct ICoreWebView2WebMessageReceivedEventHandler;

namespace litehtml {
	class document;
}

namespace ocr {
	class HTMLDocument;

	struct DictionaryEntry {
		std::string entry{};
		std::string entry_html{};
	};

	struct DictionaryData {
		std::vector<DictionaryEntry> entries{};
		int                          width{-1};
	};

	struct OCRBlock {
		std::vector<OCRResultPacked> results{};
		bool                         horizontal;
		std::vector<cv::Point>       poly{};
	};

	class TooltipWnd {
	private:
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
		std::optional<std::chrono::time_point<std::chrono::steady_clock>> start;

		std::vector<OCRBlock>                           results{};
		std::size_t                                     results_size{};
		std::unique_ptr<mdict::Mdict>                   mdict;
		std::string                                     css_data;
		std::unordered_map<std::string, DictionaryData> dictionary_data{};
		int max_webpage_width{scroll_bar_width};

		Microsoft::WRL::ComPtr<ID2D1Factory>          d2d1_factory             = nullptr;
		Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target            = nullptr;
		Microsoft::WRL::ComPtr<IDWriteFactory>        direct_write_factory     = nullptr;

		std::shared_ptr<litehtml::document> html_renderer;
		std::unique_ptr<HTMLDocument> html_doc_impl;

		void initDirectWrite();

		void cleanupDirectWrite();

		void initDictionary(const std::string& dict_string_path);

		void updateWindowSize() const;

		void updateWindowPosition() const;

		void refreshWindow();

		void onWebsiteChanged();

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

		static std::unique_ptr<TooltipWnd> initTooltip(
			const std::vector<OCRResult>& res,
			const cv::Rect&               rect,
			const std::string&            dict_folder_path
		);

		void updateRectRes(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

		void updateLoop();

		std::pair<int, int> getExtent() const;
	};
}
