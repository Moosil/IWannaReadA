#pragma once

#include <qgraphicsview.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <opencv2/core/types.hpp>

#include "screenshot_viewer.h"

namespace iwra {
	class ScreenshotWindow : public QMainWindow {
		Q_OBJECT

		bool              is_dragging = false;
		QPoint            start{-1, -1};
		QPoint            end{-1, -1};
		QPixmap           desktop;
		QGraphicsScene*   scene;
		QGraphicsView*    view;
		ScreenshotViewer* screenshot_viewer;

	public:
		ScreenshotWindow() = delete;

		explicit ScreenshotWindow(QWidget* parent);

		[[nodiscard]] QPixmap captureEntireScreen() const;

		[[nodiscard]] QPixmap captureScreenRegion(cv::Rect capture_rect) const;

		static cv::Mat QPixmap2cvMat(const QPixmap& pixmap);

		void updateScreenshotLabel() const;

	protected:
		void mousePressEvent(QMouseEvent* event) override;

		void mouseMoveEvent(QMouseEvent* event) override;

		void mouseReleaseEvent(QMouseEvent* event) override;

		void showEvent(QShowEvent* event) override;

	signals:
		void activated(const cv::Mat& screenshot_mat, const cv::Rect& rect);
	};
}
