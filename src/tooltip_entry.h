#pragma once

#include <clip.h>
#include <memory>
#include <qboxlayout.h>
#include <qlabel.h>
#include <qwidget.h>
#include <qpushbutton.h>

#include "anki_connect.h"
#include "dict_parser.h"

namespace iwra {
	class TooltipEntry : public QWidget {
	public:
		TooltipEntry(QWidget* parent, const std::shared_ptr<AnkiInterface>& p_interface);

		void update(const DictionaryParser::Entry& p_entry, const std::string& p_phrase, const std::string& p_sentence);

		void hideHeadwordLayoutItem(int column) const;

		void addSpacerToHeadword(int index, int size) const;

		void addLabelToHeadword(int index, const std::string& pinyin, const std::string& hanzi) const;

	private:
		std::shared_ptr<AnkiInterface> anki_interface;
		bool                           anki_connected;

		DictionaryParser::Entry entry;
		std::string             phrase;
		std::string             sentence;

		QVBoxLayout* layout;

		QWidget*     title_bar;
		QHBoxLayout* title_bar_layout;
		QPushButton* anki_button;

		QWidget*     headword;
		QHBoxLayout* headword_layout;

		QLabel* definitions;

		void setClipboardCharacter() const {
			clip::set_text(entry.getSimp());
		}

		void setClipboardPhrase() const {
			clip::set_text(phrase);
		}

		void setClipboardSentence() const {
			clip::set_text(sentence);
		}

		void addToAnki() const {
			if (!anki_interface) {
				return;
			}

			if (anki_connected) {
				anki_interface->addNote(
					entry.getSimp(),
					entry.getPinyin(),
					entry.definitions | std::views::join_with('\n') | std::ranges::to<std::string>(),
					sentence
				);
			} else {
				anki_interface->checkConnection();
			}

			if (!anki_connected && anki_interface->getConnected()) {
				anki_button->setIcon(QIcon("../assets/add_to_anki.png"));
				anki_button->setToolTip("Click to add current entry to Anki");
			}
			if (anki_connected && !anki_interface->getConnected()) {
				anki_button->setIcon(QIcon("../assets/retry_connection.png"));
				anki_button->setToolTip("Click to retry Anki connection");
			}
		}

		[[nodiscard]] QVBoxLayout* getLayoutLabel(const std::string& pinyin, const std::string& hanzi) const;

		[[nodiscard]] QLabel* getHanziLabel(const std::string& text) const;

		[[nodiscard]] QLabel* getPinyinLabel(const std::string& text) const;

	protected:
		void contextMenuEvent(QContextMenuEvent* event) override;
	};
}
