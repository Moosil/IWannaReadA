#pragma once

#include <qlabel.h>
#include <qwidget.h>
#include <qlayout.h>

#include "dict_parser.h"


namespace iwra {
	class TooltipEntry : public QWidget {
	public:
		explicit TooltipEntry(QWidget* parent);

		void update(const DictParser::entry& entry);
	private:
		QVBoxLayout* layout;
		QLabel* hanzi{};
		QLabel* pinyin{};

		std::vector<QLabel*> definitions{};
	};
}
