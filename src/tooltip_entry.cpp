#include "tooltip_entry.h"

#include <qevent.h>
#include <qmenu.h>
#include <spdlog/spdlog.h>

#include "util_qt.h"

namespace iwra {
	TooltipEntry::TooltipEntry(QWidget* parent, const std::shared_ptr<AnkiInterface>& p_interface):
		QWidget{parent},
		anki_interface{p_interface},
		layout{new QVBoxLayout(this)},
		headword{new QWidget(this)},
		headword_layout{new QGridLayout(headword)},
		definitions{new QLabel(this)} { {
			QFont font = definitions->font();
			font.setPointSize(10);
			definitions->setFont(font);
		}

		definitions->setWordWrap(true);

		layout->addWidget(headword);
		layout->addSpacing(8);
		layout->addWidget(definitions);

		compactifyWidget(headword);
		definitions->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
		definitions->setContentsMargins(4, 0, 4, 0);
		compactifyWidget(this);

		compactifyLayout(layout);
		compactifyLayout(headword_layout);
	}

	void TooltipEntry::update(
		const DictionaryParser::Entry& p_entry,
		const std::string&             p_phrase,
		const std::string&             p_sentence
	) {
		entry    = p_entry;
		phrase   = p_phrase;
		sentence = p_sentence;

		int index = 0;
		for (const auto& [characters] : p_entry.words) {
			for (const auto& [simp, _, pinyin] : characters) {
				addLabelToHeadword(0, index, pinyin);
				addLabelToHeadword(1, index, simp);
				++index;
			}
			while (index < headword_layout->columnCount() && !headword_layout->itemAtPosition(0, index)->spacerItem()) {
				hideHeadwordLayoutItem(0, index);
				hideHeadwordLayoutItem(1, index);
				++index;
			}
			addSpacerToHeadword(0, index, 4);
			addSpacerToHeadword(1, index, 4);
			++index;
		}

		--index;
		addSpacerToHeadword(0, index, 8);
		addSpacerToHeadword(1, index, 8);
		++index;

		for (const auto& [characters] : p_entry.words) {
			for (const auto& [_, trad, pinyin] : characters) {
				addLabelToHeadword(0, index, pinyin);
				addLabelToHeadword(1, index, trad);
				++index;
			}
			while (index < headword_layout->columnCount() && !headword_layout->itemAtPosition(0, index)->spacerItem()) {
				hideHeadwordLayoutItem(0, index);
				hideHeadwordLayoutItem(1, index);
				++index;
			}
			addSpacerToHeadword(0, index, 4);
			addSpacerToHeadword(1, index, 4);
			++index;
		}

		--index;
		for (; index < headword_layout->columnCount(); ++index) {
			hideHeadwordLayoutItem(0, index);
			hideHeadwordLayoutItem(1, index);
		}

		using namespace std::string_literals;
		const std::string definition_concat = "<ul style=\"margin-left:10px; -qt-list-indent:0;\"><li>" + (
			                                      entry.definitions | std::views::join_with("</li><li>"s) |
			                                      std::ranges::to<std::string>()) + "</li></ul>";
		definitions->setText(QString::fromStdString(definition_concat));
	}

	void TooltipEntry::hideHeadwordLayoutItem(const int row, const int column) const {
		QLayoutItem* curr = headword_layout->itemAtPosition(row, column);
		if (!curr) {
			return;
		}

		if (curr->widget()) {
			curr->widget()->hide();
		} else if (curr->spacerItem()) {
			curr->spacerItem()->changeSize(0, 0);
		} else {
			spdlog::warn("malformed QLayoutItem ({}, {}) in TooltipEntry", row, column);
		}
	}

	void TooltipEntry::addSpacerToHeadword(const int row, const int column, const int size) const {
		QLayoutItem* spacer = headword_layout->itemAtPosition(row, column);
		if (!spacer) {
			headword_layout->addItem(new QSpacerItem(size, 0), row, column);
			return;
		}

		if (QSpacerItem* curr_spacer = spacer->spacerItem()) {
			curr_spacer->changeSize(size, 0);
		} else if (spacer->widget()) {
			headword_layout->addItem(new QSpacerItem(size, 0), row, column);
		} else {
			spdlog::warn("malformed QLayoutItem ({}, {}) in TooltipEntry", row, column);
			headword_layout->addItem(new QSpacerItem(size, 0), row, column);
		}
	}

	void TooltipEntry::addLabelToHeadword(const int row, const int column, const std::string& text) const {
		QLayoutItem* label = headword_layout->itemAtPosition(row, column);

		if (!label) {
			auto* new_label = getPinyinLabel();
			new_label->setText(QString::fromStdString(text));
			headword_layout->addWidget(new_label, row, column);
			return;
		}

		if (QWidget* curr_widget = label->widget()) {
			if (auto* curr_label = qobject_cast<QLabel*>(curr_widget)) {
				curr_label->setText(QString::fromStdString(text));
				curr_label->show();
			} else {
				spdlog::warn("non-QLabel QWidget in QLayoutItem ({}, {}) in TooltipEntry", row, column);
				auto* new_label = getPinyinLabel();
				new_label->setText(QString::fromStdString(text));
				headword_layout->addWidget(new_label, row, column);
			}
		} else if (label->spacerItem()) {
			auto* new_label = getPinyinLabel();
			new_label->setText(QString::fromStdString(text));
			headword_layout->addWidget(new_label, row, column);
		} else {
			spdlog::warn("malformed QLayoutItem ({}, {}) in TooltipEntry", row, column);
			auto* new_label = getPinyinLabel();
			new_label->setText(QString::fromStdString(text));
			headword_layout->addWidget(new_label, row, column);
		}
	}

	QLabel* TooltipEntry::getHanziLabel() const {
		auto* res = new QLabel(headword);

		QFont font = res->font();
		font.setPointSize(20);
		res->setFont(font);

		res->setAlignment(Qt::AlignCenter);

		compactifyLabel(res);

		return res;
	}

	QLabel* TooltipEntry::getPinyinLabel() const {
		auto* res = new QLabel(headword);

		QFont font = res->font();
		font.setPointSize(14);
		res->setFont(font);

		res->setAlignment(Qt::AlignCenter);

		compactifyLabel(res);

		return res;
	}

	void TooltipEntry::contextMenuEvent(QContextMenuEvent* event) {
		QMenu menu(this);

		const QAction* copy_character_action = menu.addAction("copy character");
		connect(
			copy_character_action,
			&QAction::triggered,
			this,
			[this]() {
				setClipboardCharacter();
			}
		);

		const QAction* copy_phrase_action = menu.addAction("copy phrase");
		connect(
			copy_phrase_action,
			&QAction::triggered,
			this,
			[this]() {
				setClipboardPhrase();
			}
		);

		const QAction* copy_sentence_action = menu.addAction("copy sentence");
		connect(
			copy_sentence_action,
			&QAction::triggered,
			this,
			[this]() {
				setClipboardSentence();
			}
		);

		if (anki_interface) {
			const QAction* add_to_anki_action = menu.addAction("add_to_anki");
			connect(
				add_to_anki_action,
				&QAction::triggered,
				this,
				[this]() {
					addToAnki();
				}
			);
		}

		menu.exec(event->globalPos());

		QWidget::contextMenuEvent(event);
	}
}

