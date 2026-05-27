#include "screenshot_viewer.h"

#include <qpainter.h>

#include "util.h"

namespace iwra {
	ScreenshotViewer::ScreenshotViewer(QGraphicsItem* parent):
		QGraphicsItem{parent} {}

	QRectF ScreenshotViewer::boundingRect() const {
		const auto [width, height] = getScreenSize();
		return {0, 0, static_cast<qreal>(width), static_cast<qreal>(height)};
	}

	void ScreenshotViewer::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
		painter->drawPixmap(0, 0, pixmap);

		QPainterPath path;
		path.setFillRule(Qt::OddEvenFill);
		path.addRect(boundingRect());
		path.addRect(rect);

		painter->setBrush(QColor(0, 0, 0, 120));
		painter->setPen(Qt::NoPen);
		painter->drawPath(path);

		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Qt::white, 2));
		painter->drawRect(rect);
	}

	void ScreenshotViewer::updatePixmapRect(const QPixmap& new_pixmap, const QRect& new_rect) {
		pixmap = new_pixmap;
		rect   = new_rect;
		update();
	}
}
