#include "screenshot_viewer.h"

#include <qpainter.h>

#include "util.h"


namespace iwra {
	ScreenshotViewer::ScreenshotViewer(QGraphicsItem* parent): QGraphicsItem{parent} {}

	QRectF ScreenshotViewer::boundingRect() const {
		const auto [width, height] = getScreenSize();
		return {0, 0, static_cast<qreal>(width), static_cast<qreal>(height)};
	}

	void  ScreenshotViewer::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
		painter->drawPixmap(boundingRect(), pixemap, boundingRect());
		painter->setBrush(QColor(0,0,0,127));

		const auto [width, height] = getScreenSize();

		painter->drawRect(0, 0, width, rect.y());
		painter->drawRect(0, rect.top(), width, height - rect.top());

		painter->drawRect(0, rect.y(), rect.x(), rect.top());
		painter->drawRect(rect.right(), rect.y(), width - rect.right(), rect.top());
	}
}
