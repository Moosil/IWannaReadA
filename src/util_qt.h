#pragma once

#include <qlabel.h>
#include <qlayout.h>

namespace iwra {
	inline void compactifyWidget(QWidget* widget) {
		widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
		widget->setContentsMargins(0, 0, 0, 0);
	}

	inline void compactifyLabel(QLabel* label) {
		label->setFixedHeight(label->fontMetrics().ascent());
		label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
		label->setContentsMargins(0, 0, 0, 0);
	}

	inline void compactifyLayout(QLayout* layout) {
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
	}
}
