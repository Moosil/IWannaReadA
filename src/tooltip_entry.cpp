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
		title_bar{new QWidget(this)},
		title_bar_layout{new QHBoxLayout(title_bar)},
		anki_button{new QPushButton(title_bar)},
		headword{new QWidget(title_bar)},
		headword_layout{new QHBoxLayout(headword)},
		definitions{new QLabel(this)} { {
			QFont font = definitions->font();
			font.setPointSize(10);
			definitions->setFont(font);
		}

		anki_button->setFlat(true);
		connect(
			anki_button,
			&QPushButton::pressed,
			this,
			[this]() {
				addToAnki();
			}
		);

		if (anki_interface->getConnected()) {
			anki_button->setIcon(QIcon("../assets/add_to_anki.png"));
			anki_button->setToolTip("Click to add current entry to Anki");
			anki_connected = true;
		} else {
			anki_button->setIcon(QIcon("../assets/retry_connection.png"));
			anki_button->setToolTip("Click to retry Anki connection");
			anki_connected = false;
		}

		definitions->setWordWrap(true);

		title_bar_layout->addWidget(headword);
		title_bar_layout->addWidget(anki_button, 0, Qt::AlignRight | Qt::AlignVCenter);

		layout->addWidget(title_bar);
		layout->addSpacing(8);
		layout->addWidget(definitions);

		compactifyWidget(anki_button);
		definitions->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
		definitions->setContentsMargins(4, 0, 4, 0);

		compactifyWidget(this);
		title_bar->setContentsMargins(0, 0, 0, 0);
		title_bar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
		compactifyWidget(headword);

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
			for (const auto& [simp, trad, pinyin] : characters) {
				addLabelToHeadword(index, pinyin, simp);
				++index;
			}
			while (index < headword_layout->count() && !headword_layout->itemAt(index)->spacerItem()) {
				hideHeadwordLayoutItem(index);
				++index;
			}
			addSpacerToHeadword(index, 4);
			++index;
		}

		if (index > 0) {
			--index;
			addSpacerToHeadword(index, 8);
			++index;
		}

		for (const auto& [characters] : p_entry.words) {
			for (const auto& [simp, trad, pinyin] : characters) {
				addLabelToHeadword(index, pinyin, trad);
				++index;
			}
			while (index < headword_layout->count() && !headword_layout->itemAt(index)->spacerItem()) {
				hideHeadwordLayoutItem(index);
				++index;
			}
			addSpacerToHeadword(index, 4);
			++index;
		}

		if (index > 0) {
			--index;
		}
		for (; index < headword_layout->count(); ++index) {
			hideHeadwordLayoutItem(index);
		}

		using namespace std::string_literals;
		const std::string definition_concat = "<ul style=\"margin-left:10px; -qt-list-indent:0;\"><li>" + (
			                                      entry.definitions | std::views::join_with("</li><li>"s) |
			                                      std::ranges::to<std::string>()) + "</li></ul>";
		definitions->setText(QString::fromStdString(definition_concat));
	}

	void TooltipEntry::hideHeadwordLayoutItem(const int column) const {
		QLayoutItem* curr = headword_layout->itemAt(column);
		if (!curr) {
			return;
		}

		if (curr->layout()) {
			curr->layout()->itemAt(0)->widget()->hide();
			curr->layout()->itemAt(1)->widget()->hide();
		} else if (curr->spacerItem()) {
			curr->spacerItem()->changeSize(0, 0);
		} else {
			spdlog::warn("malformed QLayoutItem ({}) in TooltipEntry", column);
		}
	}

	void TooltipEntry::addSpacerToHeadword(const int index, const int size) const {
		if (headword_layout->count() <= index) {
			headword_layout->addItem(new QSpacerItem(size, 0));
			return;
		}

		QLayoutItem* spacer = headword_layout->itemAt(index);

		if (QSpacerItem* curr_spacer = spacer->spacerItem()) {
			curr_spacer->changeSize(size, 0);
		} else if (spacer->layout()) {
			headword_layout->insertItem(index, new QSpacerItem(size, 0));
		} else {
			spdlog::warn("malformed QLayoutItem ({}) in TooltipEntry", index);
			headword_layout->insertItem(index, new QSpacerItem(size, 0));
		}
	}

	void TooltipEntry::addLabelToHeadword(const int index, const std::string& pinyin, const std::string& hanzi) const {
		if (headword_layout->count() <= index) {
			headword_layout->addLayout(getLayoutLabel(pinyin, hanzi));
			return;
		}

		QLayoutItem* label_item = headword_layout->itemAt(index);

		if (const QLayout* label_layout = label_item->layout()) {
			if (QWidget* pinyin_widget = label_layout->itemAt(0)->widget();
				auto*    pinyin_label  = qobject_cast<QLabel*>(pinyin_widget)) {
				pinyin_label->setText(QString::fromStdString(pinyin));
				pinyin_label->show();
			} else {
				spdlog::warn("non-QLabel QWidget in QLayoutItem ({}) in TooltipEntry", index);
			}
			if (QWidget* hanzi_widget = label_layout->itemAt(1)->widget();
				auto*    pinyin_label = qobject_cast<QLabel*>(hanzi_widget)) {
				pinyin_label->setText(QString::fromStdString(hanzi));
				pinyin_label->show();
			} else {
				spdlog::warn("non-QLabel QWidget in QLayoutItem ({}) in TooltipEntry", index);
			}
		} else if (label_item->spacerItem()) {
			headword_layout->insertLayout(index, getLayoutLabel(pinyin, hanzi));
		} else {
			spdlog::warn("malformed QLayoutItem ({}) in TooltipEntry", index);
			headword_layout->insertLayout(index, getLayoutLabel(pinyin, hanzi));
		}
	}

	QVBoxLayout* TooltipEntry::getLayoutLabel(const std::string& pinyin, const std::string& hanzi) const {
		auto* res = new QVBoxLayout();

		res->addWidget(getPinyinLabel(pinyin));
		res->addWidget(getHanziLabel(hanzi));
		compactifyLayout(res);

		return res;
	}

	QLabel* TooltipEntry::getHanziLabel(const std::string& text) const {
		auto* res = new QLabel(headword);

		QFont font = res->font();
		font.setPointSize(20);
		res->setFont(font);

		res->setAlignment(Qt::AlignCenter);

		compactifyLabel(res);

		res->setText(QString::fromStdString(text));

		return res;
	}

	QLabel* TooltipEntry::getPinyinLabel(const std::string& text) const {
		auto* res = new QLabel(headword);

		QFont font = res->font();
		font.setPointSize(14);
		res->setFont(font);

		res->setAlignment(Qt::AlignCenter);

		compactifyLabel(res);

		res->setText(QString::fromStdString(text));

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

