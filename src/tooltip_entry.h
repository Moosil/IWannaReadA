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
		TooltipEntry(
			QWidget*                              parent,
			const std::shared_ptr<AnkiInterface>& interface,
			const std::shared_ptr<Config>&        config);

		void update(
			const DictionaryParser::Entry& p_entry,
			const std::string&             p_phrase,
			const std::string&             p_sentence,
			long long                      p_offset);

		void hideHeadwordLayoutItem(int column) const;

		void addSpacerToHeadword(int index, int size) const;

		void addLabelToHeadword(int index, const std::string& pinyin, const std::string& hanzi) const;

	private:
		std::shared_ptr<AnkiInterface> anki_interface;
		bool                           anki_connected;
		std::shared_ptr<Config>        config;

		DictionaryParser::Entry entry;
		std::string             phrase;
		std::string             sentence;
		long long               offset;

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

		void addToAnki() const;

		[[nodiscard]] QVBoxLayout* getLayoutLabel(const std::string& pinyin, const std::string& hanzi) const;

		[[nodiscard]] QLabel* getHanziLabel(const std::string& text) const;

		[[nodiscard]] QLabel* getPinyinLabel(const std::string& text) const;

	protected:
		void contextMenuEvent(QContextMenuEvent* event) override;
	};
}
