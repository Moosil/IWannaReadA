#include "tooltip_entry.h"

#include <spdlog/spdlog.h>
#include "util_qt.h"


namespace iwra {
	TooltipEntry::TooltipEntry(QWidget* parent):
		QWidget{parent},

		layout{new QVBoxLayout(this)},

		headword{new QWidget(this)},
		headword_layout{new QHBoxLayout(headword)},

		simp_headword{new QWidget(headword)},
		simp_headword_layout{new QVBoxLayout(simp_headword)},
		simp_hanzi{new QLabel(simp_headword)},
		simp_pinyin{new QLabel(simp_headword)},

		trad_headword{new QWidget(headword)},
		trad_headword_layout{new QVBoxLayout(trad_headword)},
		trad_hanzi{new QLabel(trad_headword)},
		trad_pinyin{new QLabel(trad_headword)},

		definitions{new QLabel(this)}
	{
		{
			QFont hanzi_font = simp_hanzi->font();
			hanzi_font.setPointSize(20);

			QFont pinyin_font = simp_hanzi->font();
			pinyin_font.setPointSize(14);

			QFont definition_font = simp_hanzi->font();
			definition_font.setPointSize(10);

			simp_hanzi->setFont(hanzi_font);
			trad_hanzi->setFont(hanzi_font);

			simp_pinyin->setFont(pinyin_font);
			trad_pinyin->setFont(pinyin_font);

			definitions->setFont(definition_font);

		}

		// debug-layout
		// simp_hanzi->setStyleSheet("background-color: red;");
		// trad_hanzi->setStyleSheet("background-color: red;");
		// simp_pinyin->setStyleSheet("background-color: orange;");
		// trad_pinyin->setStyleSheet("background-color: orange;");
		//
		// simp_headword->setStyleSheet("background-color: green;");
		// trad_headword->setStyleSheet("background-color: green;");
		// headword->setStyleSheet("background-color: blue;");
		//
		// setStyleSheet("background-color: purple;");

		simp_hanzi->setAlignment(Qt::AlignJustify | Qt::AlignBottom);
		trad_hanzi->setAlignment(Qt::AlignJustify | Qt::AlignBottom);
		simp_pinyin->setAlignment(Qt::AlignJustify | Qt::AlignBottom);
		trad_pinyin->setAlignment(Qt::AlignJustify | Qt::AlignBottom);

		definitions->setWordWrap(true);

		simp_headword_layout->addWidget(simp_pinyin, 0, Qt::AlignHCenter | Qt::AlignBottom);
		simp_headword_layout->addWidget(simp_hanzi, 0, Qt::AlignHCenter | Qt::AlignBottom);

		trad_headword_layout->addWidget(trad_pinyin, 0, Qt::AlignHCenter | Qt::AlignBottom);
		trad_headword_layout->addWidget(trad_hanzi, 0, Qt::AlignHCenter | Qt::AlignBottom);

		headword_layout->addWidget(simp_headword);
		headword_layout->addSpacing(16);
		headword_layout->addWidget(trad_headword);

		layout->addWidget(headword);
		layout->addSpacing(8);
		layout->addWidget(definitions);

		compactifyLabel(simp_hanzi);
		compactifyLabel(trad_hanzi);

		compactifyLabel(simp_pinyin);
		compactifyLabel(trad_pinyin);

		compactifyWidget(headword);
		compactifyWidget(simp_headword);
		compactifyWidget(trad_headword);
		definitions->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
		definitions->setContentsMargins(4, 0, 4, 0);
		compactifyWidget(this);

		compactifyLayout(layout);
		compactifyLayout(headword_layout);
		compactifyLayout(simp_headword_layout);
		compactifyLayout(trad_headword_layout);
	}

	void TooltipEntry::update(const DictParser::entry& entry) const {
		simp_hanzi->setText(QString::fromStdString(entry.get_simp()));
		simp_pinyin->setText(QString::fromStdString(entry.get_pinyin()));
		trad_hanzi->setText(QString::fromStdString(entry.get_simp()));
		trad_pinyin->setText(QString::fromStdString(entry.get_pinyin()));

		using namespace std::string_literals;
		const std::string definition_concat = entry.definitions | std::views::join_with("\n • "s) | std::ranges::to<std::string>();
		definitions->setText(" • " + QString::fromStdString(definition_concat));
	}
}

