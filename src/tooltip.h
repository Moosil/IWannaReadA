#pragma once

#include <qhotkey.h>
#include <unordered_map>
#include <qmainwindow.h>

#include "anki_connect.h"
#include "common.h"
#include "dict_parser.h"
#include "tooltip_entry.h"

namespace iwra {
	struct DictionaryData {
		std::vector<DictParser::entry> entries{};
		std::string                    phrase;
	};

	struct OCRBlock {
		std::vector<OCRResultPacked> results;
		std::vector<cv::Point>       poly;
		bool                         horizontal;
	};

	class TooltipWnd : public QMainWindow {
	public:
		TooltipWnd() = delete;

		TooltipWnd(
			QWidget* parent,
			const std::shared_ptr<DictParser>&      parser,
			const std::shared_ptr<Anki::Interface>& anki
		);

		void updateResRect(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

	private:
		cv::Rect         rect;
		OCRBlock*        current_block{nullptr};
		OCRResultPacked* current_word{nullptr};
		std::string current_phrase{};

		QHotkey* hover_hotkey;
		bool     is_hovering{false};

		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::shared_ptr<DictParser>                     parser;
		std::unordered_map<std::string, DictionaryData> dictionary_data;

		std::vector<TooltipEntry*> entries{};

		std::shared_ptr<Anki::Interface> anki;

		bool initDictEntry(const std::string& key, const std::string& phrase);

		static void processOCRResults(
			const std::vector<OCRResult>& res,
			const cv::Point&              topleft,
			std::vector<OCRBlock>&        out
		);

		static std::vector<OCRResultPacked> ocrSplitText(const Poly2I& rect, const Text& text, bool horizontal);

		void updateWindowPosition();

		const DictionaryData* getDictDataOrInit(const std::string& key, const std::string& phrase);

		void updateWindowEntry(const DictionaryData* dict_data, const std::string& phrase, const std::string& sentence);

		void refreshWindow();

		void refreshHovering();

		void addAnkiCard(
			const std::string& character,
			const std::string& phrase,
			const std::string& pinyin,
			const std::string& sentence,
			const std::string& definition
		) const;

		static std::string getSentence(OCRBlock* hover_block);

		static std::string getPhrase(const OCRResultPacked* hover_word, const OCRBlock* hover_block);
	};
}
