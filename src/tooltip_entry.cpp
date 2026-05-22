#include "tooltip_entry.h"




namespace iwra {
	TooltipEntry::TooltipEntry(QWidget* parent):
		QWidget{parent},
		layout{new QVBoxLayout(this)},
		hanzi{new QLabel(this)},
		pinyin{new QLabel(this)}
	{
		setLayout(layout);
		layout->addWidget(hanzi);
		layout->addWidget(pinyin);
	}

	void TooltipEntry::update(const DictParser::entry& entry) {
		hanzi->setText(QString::fromStdString(entry.get_simp()));
		pinyin->setText(QString::fromStdString(entry.get_pinyin()));
		std::size_t i = 0;
		for (; i < entry.definitions.size(); ++i) {
			if (i < definitions.size()) {
				definitions[i]->show();
				definitions[i]->setText(QString::fromStdString(entry.definitions[i]));
			} else {
				auto* new_definition = new QLabel(this);
				layout->addWidget(new_definition);
				definitions.push_back(new_definition);
				definitions[i]->setText(QString::fromStdString(entry.definitions[i]));
			}
		}
		for (; i < definitions.size(); ++i) {
			definitions[i]->hide();
		}
	}
}

