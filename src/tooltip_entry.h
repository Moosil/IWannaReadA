#pragma once

#include <clip.h>
#include <qlabel.h>
#include <qwidget.h>
#include <qboxlayout.h>

#include <memory>

#include "anki_connect.h"
#include "dict_parser.h"


namespace iwra {
	class TooltipEntry : public QWidget {
	public:
		TooltipEntry(QWidget* parent, const std::shared_ptr<Anki::Interface>& p_interface);

		void update(const DictParser::entry& p_entry, const std::string& p_phrase, const std::string& p_sentence);

		void hideHeadwordLayoutItem(int row, int column) const;

		void addSpacerToHeadword(int row, int column, int size) const;

		void addLabelToHeadword(int row, int column, const std::string& text) const;

	private:
		std::shared_ptr<Anki::Interface> anki_interface;
		DictParser::entry entry;
		std::string phrase;
		std::string sentence;

		QVBoxLayout* layout;

		QWidget* headword;
		QGridLayout* headword_layout;

		QLabel* definitions;

		void set_clipboard_character() const {
			clip::set_text(entry.get_simp());
		}

		void set_clipboard_phrase() const {
			clip::set_text(phrase);
		}

		void set_clipboard_sentence() const {
			clip::set_text(sentence);
		}

		void add_to_anki() const {
			anki_interface->add_note(
				entry.get_simp(),
				entry.get_pinyin(),
				entry.definitions | std::views::join_with('\n') | std::ranges::to<std::string>(),
				sentence
			);
		}

		QLabel* getHanziLabel() const;

		QLabel* getPinyinLabel() const;

	protected:
		void contextMenuEvent(QContextMenuEvent* event) override;
	};
}
