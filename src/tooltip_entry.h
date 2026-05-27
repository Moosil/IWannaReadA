#pragma once

#include <clip.h>
#include <memory>
#include <qboxlayout.h>
#include <qlabel.h>
#include <qwidget.h>

#include "anki_connect.h"
#include "dict_parser.h"

namespace iwra {
	class TooltipEntry : public QWidget {
	public:
		TooltipEntry(QWidget* parent, const std::shared_ptr<AnkiInterface>& p_interface);

		void update(const DictionaryParser::Entry& p_entry, const std::string& p_phrase, const std::string& p_sentence);

		void hideHeadwordLayoutItem(int row, int column) const;

		void addSpacerToHeadword(int row, int column, int size) const;

		void addLabelToHeadword(int row, int column, const std::string& text) const;

	private:
		std::shared_ptr<AnkiInterface> anki_interface;
		DictionaryParser::Entry        entry;
		std::string                    phrase;
		std::string                    sentence;

		QVBoxLayout* layout;

		QWidget*     headword;
		QGridLayout* headword_layout;

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
			anki_interface->add_note(
				entry.getSimp(),
				entry.getPinyin(),
				entry.definitions | std::views::join_with('\n') | std::ranges::to<std::string>(),
				sentence
			);
		}

		[[nodiscard]] QLabel* getHanziLabel() const;

		[[nodiscard]] QLabel* getPinyinLabel() const;

	protected:
		void contextMenuEvent(QContextMenuEvent* event) override;
	};
}
