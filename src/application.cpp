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
		is_refresh_enabled{config.getRefresh()},
		screenshot_hotkey{new QHotkey(QKeySequence("ctrl+d"), true, this)},
		screenshot_window{new ScreenshotWindow(nullptr)},
		tooltip_window{new TooltipWindow(nullptr, config)} {
		// start function definition
		ocr_engine = OCREngine::create(config);
		if (!ocr_engine) {
			throw std::runtime_error("OCREngine::create failed");
		}

		screenshot_window->hide();
		tooltip_window->hide();

		if (const std::optional refresh_interval_opt = config.getRefreshIntervalMs();
			refresh_interval_opt.has_value()) {
			refresh_interval = refresh_interval_opt.value();
		} else {
			is_refresh_enabled = false;
		}

		if (const std::optional style_path = config.getStyle();
			style_path.has_value()) {
			if (std::ifstream file_stream{style_path.value()};
				!file_stream.is_open()) {
				spdlog::warn("failed to open style (qss) file at {}", style_path.value().string());
			} else {
				const std::string content((std::istreambuf_iterator(file_stream)), std::istreambuf_iterator<char>());
				setStyleSheet(QString::fromStdString(content));
			}
		}

		connect(
			screenshot_window,
			&ScreenshotWindow::activated,
			this,
			[this](const cv::Mat& screenshot_mat, const cv::Rect& rect) {
				curr_screenshot_rect = rect;
				screenshot_window->hide();
				std::ignore = processScreenshot(screenshot_mat, rect);

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
				const auto res = ocr_engine->run(copy);
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

		std::ignore = processScreenshot(cv_mat, curr_screenshot_rect);

		QApplication::timerEvent(event);
	}
}
