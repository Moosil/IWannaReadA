#include "screenshot.h"

#include <qevent.h>
#include <qgraphicsview.h>
#include <qscreen.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>
#include <spdlog/spdlog.h>

#include "util.h"


namespace iwra {
	ScreenshotWnd::ScreenshotWnd(QWidget* parent, cv::Mat* screenshot_mat, cv::Rect* screenshot_rect) :
		QMainWindow(parent),
		desktop(captureEntireScreen()),
		mat{screenshot_mat},
		rect{screenshot_rect},
		view(new QGraphicsView),
		scene(new QGraphicsScene),
		screenshot_viewer(new ScreenshotViewer) {
		scene->addItem(screenshot_viewer);
		view->setScene(scene);
		setCentralWidget(view);
	}

	QPixmap ScreenshotWnd::captureEntireScreen() const {
		const auto [width, height] = getScreenSize();
		return captureScreenRegion(cv::Rect(0, 0, width, height));
	}

	QPixmap ScreenshotWnd::captureScreenRegion(const cv::Rect capture_rect) const {
		return this->screen()->grabWindow(0, capture_rect.x, capture_rect.y, capture_rect.width, capture_rect.height);
	}

	cv::Mat ScreenshotWnd::QPixmap2cvMat(const QPixmap& pixmap) {
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

	void ScreenshotWnd::updateScreenshotLabel() const {
		if (desktop.isNull()) {
			spdlog::error("screenshot failed: desktop (QPixmap) is null");
			return;
		}
		const auto [left, right] = std::minmax(start.x(), end.x());
		const auto [top, bottom] = std::minmax(start.y(), end.y());
		const auto r_width       = right - left;
		const auto r_height      = bottom - top;
		screenshot_viewer->update(top, left, r_width, r_height);
	}

	void ScreenshotWnd::mousePressEvent(QMouseEvent* event) {
		if (event->button() == Qt::LeftButton) {
			is_dragging = true;
			start       = event->pos();
		}
		QMainWindow::mousePressEvent(event);
	}

	void ScreenshotWnd::mouseMoveEvent(QMouseEvent* event) {
		if (is_dragging) {
			end = event->pos();
			updateScreenshotLabel();
		}
		QMainWindow::mouseMoveEvent(event);
	}

	void ScreenshotWnd::mouseReleaseEvent(QMouseEvent* event) {
		if (is_dragging == false && event->button() == Qt::LeftButton) {
			end = event->pos();

			if (start.x() != -1 && start.y() != -1) {
				const auto [left, right] = std::minmax(start.x(), end.x());
				const auto [top, bottom] = std::minmax(start.y(), end.y());
				const auto r_width       = right - left;
				const auto r_height      = bottom - top;
				const auto pixmap        = captureScreenRegion(cv::Rect(left, top, r_width, r_height));
				*mat                     = QPixmap2cvMat(pixmap);
				*rect                    = {left, top, r_width, r_height};
			}
		}
		QMainWindow::mouseReleaseEvent(event);
	}
}
