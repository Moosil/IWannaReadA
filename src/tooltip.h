#pragma once

#include <qboxlayout.h>
#include <qhotkey.h>
#include <qmainwindow.h>
#include <qscrollarea.h>
#include <unordered_map>

#include "anki_connect.h"
#include "common.h"
#include "config.h"
#include "dict_parser.h"
#include "tooltip_entry.h"

namespace iwra {
	struct DictionaryData {
		std::vector<DictionaryParser::Entry> entries{};
		std::string                          phrase;
	};

	// ReSharper disable once CppInconsistentNaming
	struct OCRBlock {
		std::vector<OCRResultPacked> results;
		std::vector<cv::Point>       poly;
		bool                         horizontal;
	};

	class TooltipWindow : public QMainWindow {
	public:
		TooltipWindow() = delete;

		TooltipWindow(QWidget* parent, const std::shared_ptr<Config>& config);

		void updateResRect(const std::vector<OCRResult>& new_res, const cv::Rect& new_rect);

	private:
		cv::Rect         rect;
		OCRBlock*        current_block{nullptr};
		OCRResultPacked* current_word{nullptr};
		std::string      current_phrase{};

		QHotkey* hover_hotkey;
		int      timer_id{0};
		bool     is_hovering{false};

		std::vector<OCRBlock>                           results;
		std::size_t                                     results_size{};
		std::shared_ptr<DictionaryParser>               dictionary_parser;
		std::unordered_map<std::string, DictionaryData> dictionary_data;

		QWidget*                   central_widget;
		QVBoxLayout*               layout;
		QScrollArea*               scrollbar;
		std::vector<TooltipEntry*> entries{};

		std::shared_ptr<AnkiInterface> anki_interface;

		bool initDictEntry(const std::string& key, const std::string& phrase);

		// ReSharper disable once CppInconsistentNaming
		static std::vector<OCRBlock> processOCRResults(
			const std::vector<OCRResult>& unprocessed_results,
			const cv::Point&              topleft
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

	protected:
		void timerEvent(QTimerEvent* event) override;
	};
}
