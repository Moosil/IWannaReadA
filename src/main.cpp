#include <opencv2/core/mat.hpp>
#include <spdlog/spdlog.h>
#include <qapplication.h>
#include <qtconcurrentrun.h>
#include <qhotkey.h>

#include "config.h"
#include "ocr_engine.h"
#include "screenshot.h"
#include "tooltip.h"

using namespace iwra;
using s_time = std::chrono::time_point<std::chrono::steady_clock>;

QFuture<void> processScreenshot(const cv::Mat& screenshot_mat, const cv::Rect& rect, const OCREngine& ocr_engine, TooltipWnd* tooltip_window);

int main(int argc, char* argv[]) {
	Config                      yaml{"../config.yaml"};
	const bool                  refresh            = yaml.getRefresh();
	const int                   refresh_interval   = yaml.getRefreshIntervalMs().value();
	const std::filesystem::path dict_path          = yaml.getDictPath();
	const std::string           anki_card_type     = yaml.getAnkiCardType().value();
	const std::string           anki_deck_name     = yaml.getAnkiDeckName().value();

	const auto engine = OCREngine(yaml);

	auto anki = std::make_shared<Anki::Interface>(anki_deck_name, anki_card_type);
	auto dict_parser = std::make_shared<DictParser>();
	dict_parser->load(dict_path);
	spdlog::info("loaded dictionary successfully");


	QApplication app(argc, argv);
	auto* screenshot_hotkey = new QHotkey(QKeySequence("ctrl+d"), true, &app);

	auto screenshot_window = new ScreenshotWindow(nullptr);
	screenshot_window->hide();

	auto tooltip_window = new TooltipWnd(nullptr, dict_parser, anki);
	tooltip_window->hide();


	app.connect(screenshot_window, &ScreenshotWindow::activated, &app, [tooltip_window, &engine, screenshot_window](const cv::Mat& screenshot_mat, const cv::Rect& rect) {
		screenshot_window->hide();
		processScreenshot(screenshot_mat, rect, engine, tooltip_window);
	});

	app.connect(screenshot_hotkey, &QHotkey::activated, screenshot_window, [screenshot_window]() {
		screenshot_window->showFullScreen();
		screenshot_window->raise();
		screenshot_window->activateWindow();
	});

	return app.exec();
}

QFuture<void> processScreenshot(const cv::Mat& screenshot_mat, const cv::Rect& rect, const OCREngine& ocr_engine, TooltipWnd* tooltip_window) {
	cv::Mat copy = screenshot_mat.clone();

	return QtConcurrent::run([&ocr_engine, copy]() {
		return ocr_engine.run(copy);
	}).then(QtFuture::Launch::Sync, [&rect, tooltip_window](const std::vector<OCRResult>& res) {
		tooltip_window->updateResRect(res, rect);
	});
}