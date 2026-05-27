#include "application.h"

#include <qhotkey.h>
#include <qtconcurrentrun.h>
#include <opencv2/core/mat.hpp>
#include <spdlog/spdlog.h>

#include "config.h"
#include "screenshot.h"
#include "tooltip.h"

namespace iwra {
	Application::Application(int& argc, char** argv, int):
		QApplication{argc, argv},
		config{"../config.yaml"},
		ocr_engine{config},
		is_refresh_enabled{config.getRefresh()},
		refresh_interval{config.getRefreshIntervalMs().value()},
		screenshot_hotkey{new QHotkey(QKeySequence("ctrl+d"), true, this)},
		screenshot_window{new ScreenshotWindow(nullptr)},
		tooltip_window{new TooltipWindow(nullptr, config)} {
		screenshot_window->hide();
		tooltip_window->hide();

		connect(
			screenshot_window,
			&ScreenshotWindow::activated,
			this,
			[this](const cv::Mat& screenshot_mat, const cv::Rect& rect) {
				curr_screenshot_rect = rect;
				screenshot_window->hide();
				processScreenshot(screenshot_mat, rect);

				if (is_refresh_enabled) {
					if (timer_id == 0) {
						timer_id = startTimer(refresh_interval);
					} else {
						spdlog::warn("[Application] timer is already running");
					}
				}
			}
		);

		connect(
			screenshot_hotkey,
			&QHotkey::activated,
			this,
			[this]() {
				screenshot_window->showFullScreen();
				screenshot_window->raise();
				screenshot_window->activateWindow();

				if (timer_id != 0) {
					killTimer(timer_id);
					timer_id = 0;
				} else {
					spdlog::warn("[Application] can't kill timer that hasn't started");
				}
			}
		);
	}

	QFuture<void> Application::processScreenshot(const cv::Mat& screenshot_mat, const cv::Rect& rect) const {
		cv::Mat copy = screenshot_mat.clone();

		return QtConcurrent::run(
			[this, copy]() {
				const auto res = ocr_engine.run(copy);
				return res;
			}
		).then(
			QtFuture::Launch::Sync,
			[this, &rect](const std::vector<OCRResult>& res) {
				tooltip_window->updateResRect(res, rect);
			}
		);
	}

	void Application::timerEvent(QTimerEvent* event) {
		const QPixmap curr_screenshot_pixmap = screenshot_window->captureScreenRegion(curr_screenshot_rect);
		const auto    cv_mat                 = ScreenshotWindow::QPixmapToCvMat(curr_screenshot_pixmap);

		processScreenshot(cv_mat, curr_screenshot_rect);

		QApplication::timerEvent(event);
	}
}
