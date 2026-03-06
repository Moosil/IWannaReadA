#pragma once

#include "dict_extract.h"

namespace ocr {
	class 汉英词典第三版Extractor : public DictExtractor {
	public:
		[[nodiscard]] static std::string process_definition(
			const std::vector<std::string>& preprocess_def,
			const std::string&              word_class
		);

		static std::string search_for_sentence(
			lxb_html_document_t* doc,
			lxb_dom_node_t*      sentence,
			const std::string&   class_name
		);

		static void fill_sentences(
			lxb_dom_node*          curr_sentence_node,
			lxb_html_document_t*   doc,
			std::vector<Sentence>& sentences
		);

		[[nodiscard]] std::vector<EntryInfo> extractMDictHTML(const std::string& html) override;
	};
}
