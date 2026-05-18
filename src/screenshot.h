#pragma once

#include <qgraphicsview.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <opencv2/core/types.hpp>

#include "screenshot_viewer.h"

namespace iwra {
	class ScreenshotWnd : public QMainWindow {
		Q_OBJECT

		bool              is_dragging = false;
		QPoint            start{0, 0};
		QPoint            end{0, 0};
		QPixmap           desktop;
		cv::Mat*          mat;
		cv::Rect*         rect;
		QGraphicsView*    view;
		QGraphicsScene*   scene;
		ScreenshotViewer* screenshot_viewer;

	public:
		ScreenshotWnd() = delete;

		ScreenshotWnd(QWidget* parent, cv::Mat* screenshot_mat, cv::Rect* screenshot_rect);

		[[nodiscard]] QPixmap captureEntireScreen() const;

		[[nodiscard]] QPixmap captureScreenRegion(cv::Rect capture_rect) const;

		static cv::Mat QPixmap2cvMat(const QPixmap& pixmap);

		void updateScreenshotLabel() const;

	protected:
		void mousePressEvent(QMouseEvent* event) override;

		void mouseMoveEvent(QMouseEvent* event) override;

		void mouseReleaseEvent(QMouseEvent* event) override;
	};
}
