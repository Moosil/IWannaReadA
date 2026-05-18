#include <opencv2/core/mat.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Qt6/QtWidgets/QtWidgets>
#include <spdlog/spdlog.h>
#pragma comment(lib,"Shcore.lib")

#include <future>

#include "config.h"
#include "ocr_engine.h"
#include "screenshot.h"
#include "tooltip.h"
#include "log.h"

using namespace iwra;
using s_time = std::chrono::time_point<std::chrono::steady_clock>;

std::future<std::vector<OCRResult> > runOCR(
	const OCREngine& engine,
	const cv::Mat&   image
);

int main(int argc, char* argv[]) {
	Config                      yaml{"../config.yaml"};
	const bool                  refresh            = yaml.getRefresh();
	const int                   refresh_interval   = yaml.getRefreshIntervalMs().value();
	const std::filesystem::path dict_path          = yaml.getDictPath();
	const std::filesystem::path html_template_path = yaml.getHTMLTemplatePath();
	const std::string           anki_card_type     = yaml.getAnkiCardType().value();
	const std::string           anki_deck_name     = yaml.getAnkiDeckName().value();

	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	const auto engine = OCREngine(yaml);

	enum AppState {
		None = 0,
		Screenshot,
		Tooltip,
	};
	AppState state = None;
	cv::Mat                              ss;
	cv::Rect                             rect;
	std::future<std::vector<OCRResult> > pending_result;
	s_time                               prev = std::chrono::steady_clock::now();

	auto anki = std::make_shared<Anki::Interface>(anki_deck_name, anki_card_type);
	auto dict_parser = std::make_shared<DictParser>();
	dict_parser->load(dict_path);
	spdlog::info("loaded dictionary successfully");

	auto screenshot_overlay = new ScreenshotWnd(nullptr, &ss, &rect);
	screenshot_overlay->setWindowFlags(
		Qt::FramelessWindowHint |
		Qt::Tool |
		Qt::WindowStaysOnTopHint |
		Qt::NoDropShadowWindowHint
	);
	QApplication app(argc, argv);

	screenshot_overlay->showFullScreen();
	screenshot_overlay->raise();
	screenshot_overlay->activateWindow();

	auto tt_wnd = new TooltipWindow();

	return app.exec();

	try {
		while (true) {
			if (ss_wnd) {
				ss_wnd->update();

				if (!ss_wnd->is_running) {
					ss_wnd.reset();
					if (!rect.empty()) {
						const auto output = engine.run(ss);
						spdlog::info(
							"Screenshot Rect = [({}, {}), ({}, {})]",
							rect.x,
							rect.y,
							rect.x + rect.width,
							rect.y + rect.height
						);
						tt_wnd = TooltipWindow::initTooltip(output, rect, html_template_path, dict_parser, anki);
						prev   = std::chrono::steady_clock::now();
					}
				}
			} else {
				if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_HOTKEY) {
						spdlog::info("hotkey fired");
						if (!ss_wnd) {
							if (tt_wnd) {
								DestroyWindow(tt_wnd->getHwnd());
								tt_wnd.reset();
							}
							ss_wnd = ScreenshotWnd::startScreenShot(&ss, &rect);
						}
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if (tt_wnd) {
					tt_wnd->updateLoop();

					if (pending_result.valid() && pending_result.wait_for(std::chrono::seconds(0)) ==
					    std::future_status::ready) {
						const std::vector<OCRResult> results = pending_result.get();
						//spdlog::info(results | std::views::transform([](const OCRResult& r) -> std::string { return r.text.text; }) | std::views::join_with(',') | std::ranges::to<std::string>());
						tt_wnd->updateRectRes(results, rect);
					}

					if (refresh && !pending_result.valid()) {
						s_time     now                   = std::chrono::steady_clock::now();
						const auto milliseconds_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
							now - prev
						);
						if ((milliseconds_duration).count() > refresh_interval/*ms*/ &&
						    (!pending_result.valid() || pending_result.wait_for(std::chrono::seconds(0)) ==
						     std::future_status::ready)) {
							if (!rect.empty() && tt_wnd) {
								ss             = ScreenshotWnd::hBitmap2cvMat(ScreenshotWnd::captureScreenRegion(rect));
								pending_result = runOCR(engine, ss);
							}
							prev = std::chrono::steady_clock::now();
						}
					}
				}
			}
		}
	} catch (std::exception& e) {
		spdlog::critical("Exception: {}", e.what());
	}
}

std::future<std::vector<OCRResult> > runOCR(
	const OCREngine& engine,
	const cv::Mat&   image
) {
	const cv::Mat img = image.clone();
	return std::async(
		[&engine, img]() {
			return engine.run(img);
		}
	);
}
