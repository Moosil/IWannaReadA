#pragma once

#include <qlabel.h>
#include <qwidget.h>
#include <qboxlayout.h>

#include "dict_parser.h"


namespace iwra {
	class TooltipEntry : public QWidget {
	public:
		explicit TooltipEntry(QWidget* parent);

		void update(const DictParser::entry& entry) const;
	private:
		QVBoxLayout* layout;

		QWidget* headword;
		QHBoxLayout* headword_layout;

		QWidget* simp_headword;
		QVBoxLayout* simp_headword_layout;
		QLabel* simp_hanzi;
		QLabel* simp_pinyin;

		QWidget* trad_headword;
		QVBoxLayout* trad_headword_layout;
		QLabel* trad_hanzi;
		QLabel* trad_pinyin;

		QLabel* definitions;
	};
}
