#pragma once

#include <Windows.h>
#include <wil/com.h>

#include <mdict.h>
#include <unordered_map>
#include <WebView2.h>
#include <clipper2/clipper.core.h>

#include "common.h"
#include "task.h"

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
		int         width;
	};

	struct DictionaryData {
		std::vector<DictionaryEntry> entries;
		bool                         sorted{false};
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
		static constexpr int            title_bar_height = 32;
		static constexpr int            min_width        = 256;
		static constexpr int            min_height       = 256;
		static constexpr int            scroll_bar_width = 16;

		int         width{}, height{};
		bool        is_hovering{};
		Poly2I      prev_hover_rect;
		cv::Rect    rect;
		OCRBlock*   hover_block{nullptr};
		std::string hover_text;

		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::unique_ptr<mdict::Mdict>                   mdict;
		std::string                                     css_data;
		std::unordered_map<std::string, DictionaryData> dictionary_data;
		std::vector<EventRegistrationToken>             dict_init_nav_tokens;
		bool                                            inited_dictionary{false};

		wil::com_ptr<ID2D1Factory>          d2d1_factory             = nullptr;
		wil::com_ptr<ID2D1HwndRenderTarget> render_target            = nullptr;
		wil::com_ptr<ID2D1SolidColorBrush>  brush                    = nullptr;
		wil::com_ptr<IDWriteFactory>        direct_write_factory     = nullptr;
		wil::com_ptr<IDWriteTextFormat>     direct_write_text_format = nullptr;

		std::unique_ptr<WebView2::Impl>       wv_init;
		wil::com_ptr<ICoreWebView2Controller> wv_controller;
		wil::com_ptr<ICoreWebView2>           webview;
		bool                                  inited_web_view2{false};
		std::unique_ptr<task<void> >          dict_loading_task;


		bool initDirectWrite();

		void initWebView2();

		void cleanupDirectWrite();

		void loadDictionary(const std::string& dict_string_path);

		void updateWindowSize() const;

		void updateWindowPosition() const;

		void refreshWindow();

		void onWebsiteChanged();

		HRESULT onWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

		void createContextMenu(int x, int y, const std::string& phrase) const;

		task<void> loadDictHTML();

		[[nodiscard]] std::pair<float, float> getTextSize(
			const std::u16string& w_hover_text,
			float                 p_width  = FLT_MAX,
			float                 p_height = FLT_MAX
		) const;

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

		void updateLoop();
	};
}
