#include "screenshot.h"

#include <qhotkey.h>
#include <qevent.h>
#include <qgraphicsview.h>
#include <qscreen.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>
#include <spdlog/spdlog.h>

#include "util.h"


namespace iwra {
	ScreenshotWindow::ScreenshotWindow(QWidget* parent):
		QMainWindow{parent},
		scene{new QGraphicsScene(this)},
		view{new QGraphicsView(scene)},
		screenshot_viewer{new ScreenshotViewer} {
		scene->addItem(screenshot_viewer);
		setCentralWidget(view);

		view->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		setMouseTracking(true);

		setWindowFlags(
			Qt::FramelessWindowHint |
			Qt::Tool |
			Qt::WindowStaysOnTopHint |
			Qt::NoDropShadowWindowHint
		);
	}

	QPixmap ScreenshotWindow::captureEntireScreen() const {
		const auto [width, height] = getScreenSize();
		return captureScreenRegion(cv::Rect(0, 0, width, height));
	}

	QPixmap ScreenshotWindow::captureScreenRegion(const cv::Rect capture_rect) const {
		return this->screen()->grabWindow(0, capture_rect.x, capture_rect.y, capture_rect.width, capture_rect.height);
	}

	cv::Mat ScreenshotWindow::QPixmap2cvMat(const QPixmap& pixmap) {
		if (pixmap.isNull()) {
			return cv::Mat();
		}
		const QImage  image = pixmap.toImage();
		const cv::Mat mat(
			image.height(),
			image.width(),
			CV_8UC4,
			const_cast<unsigned char*>(image.constBits()),
			image.bytesPerLine()
		);

		return mat.clone();
	}

	void ScreenshotWindow::updateScreenshotLabel() const {
		if (desktop.isNull()) {
			spdlog::error("screenshot failed: desktop (QPixmap) is null");
			return;
		}
		const auto [left, right] = std::minmax(start.x(), end.x());
		const auto [top, bottom] = std::minmax(start.y(), end.y());
		const auto r_width       = right - left;
		const auto r_height      = bottom - top;
		screenshot_viewer->update_pixmap_rect(desktop, {left, top, r_width, r_height});
	}

	void ScreenshotWindow::mousePressEvent(QMouseEvent* event) {
		if (event->button() == Qt::LeftButton) {
			is_dragging = true;
			start       = event->pos();
			end = event->pos();
			updateScreenshotLabel();
		}
		QMainWindow::mousePressEvent(event);
	}

	void ScreenshotWindow::mouseMoveEvent(QMouseEvent* event) {
		if (is_dragging) {
			end = event->pos();
			updateScreenshotLabel();
		}
		QMainWindow::mouseMoveEvent(event);
	}

	void ScreenshotWindow::mouseReleaseEvent(QMouseEvent* event) {
		if (is_dragging && event->button() == Qt::LeftButton) {
			is_dragging = false;
			end = event->pos();

			if (start.x() != -1 && start.y() != -1) {
				const auto [left, right] = std::minmax(start.x(), end.x());
				const auto [top, bottom] = std::minmax(start.y(), end.y());
				const auto r_width       = right - left;
				const auto r_height      = bottom - top;
				const auto pixmap        = desktop.copy(left, top, r_width, r_height);
				emit activated(QPixmap2cvMat(pixmap), {left, top, r_width, r_height});
			}
		}
		QMainWindow::mouseReleaseEvent(event);
	}

	void ScreenshotWindow::showEvent(QShowEvent* event) {
		start = {-1, -1};
		end = {-1, -1};
		desktop = captureEntireScreen();
		updateScreenshotLabel();
	}
}
