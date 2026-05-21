#pragma once
#include <qgraphicsitem.h>


// Source - https://stackoverflow.com/a/31605276
// Posted by TheDarkKnight, modified by community. See post 'Timeline' for change history
// Retrieved 2026-05-17, License - CC BY-SA 3.0
namespace iwra {
	class ScreenshotViewer : public QGraphicsItem
	{
	public:
		explicit ScreenshotViewer(QGraphicsItem* parent = nullptr);

		QRectF boundingRect() const override;

		void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

		void update_pixmap_rect(const QPixmap& new_pixmap, const QRect& new_rect);

	private:
		QPixmap pixmap;
		QRect rect;
	};
}