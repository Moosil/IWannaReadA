#pragma once

#include <qapplication.h>
#include <qfuture.h>
#include <qhotkey.h>

#include "ocr_engine.h"
#include "screenshot.h"
#include "tooltip.h"

namespace iwra {
	class Application : public QApplication {
	public:
		#ifdef Q_QDOC
		Application(int& argc, char** argv);
		#else
		Application(int& argc, char** argv, int = ApplicationFlags);
		#endif

	private:
		Config    config;
		std::unique_ptr<OCREngine> ocr_engine;

		int  timer_id{0};
		bool is_refresh_enabled;
		int  refresh_interval;

		QHotkey* screenshot_hotkey;

		ScreenshotWindow* screenshot_window;
		TooltipWindow*    tooltip_window;

		cv::Rect curr_screenshot_rect;

		QFuture<void> processScreenshot(
			const cv::Mat&  screenshot_mat,
			const cv::Rect& rect
		) const;

	protected:
		void timerEvent(QTimerEvent* event) override;
	};
}
