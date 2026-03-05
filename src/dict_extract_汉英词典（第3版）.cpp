#include "dict_extract_汉英词典（第3版）.h"

#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>

#include "util.h"

namespace ocr {
	std::string 汉英词典第三版Extractor::process_definition(const std::string& preprocess_def, const std::string& word_class) {
		std::u16string       process_def    = utf8::utf8to16(preprocess_def);
		const std::u16string word_class_u16 = utf8::utf8to16(word_class);
		if (const std::size_t word_class_find = process_def.find(word_class_u16);
			word_class_find != std::string::npos) {
			process_def.replace(word_class_find, word_class_u16.length(), u"");
		}
		if (const std::size_t first_space = process_def.find(u' ');
			first_space != std::string::npos) {
			if (const std::size_t next_non_space = process_def.find_first_not_of(u' ', first_space);
				next_non_space != std::string::npos)
				return utf8::utf16to8(process_def.substr(next_non_space));
			return "";
		}
		return utf8::utf16to8(process_def);
	}

	std::string 汉英词典第三版Extractor::search_for_sentence(
		lxb_html_document_t* doc,
		lxb_dom_node_t*      sentence,
		const std::string&   class_name
	) {
		lxb_dom_collection_t* found_collection = searchForClass(doc, sentence, class_name);
		if (const lxb_dom_node_t* found_node = lxb_dom_collection_node(found_collection, 0);
			found_node && found_node->first_child) {
			return getTextContent(found_node->first_child);
		}
		lxb_dom_collection_destroy(found_collection, true);
		found_collection = nullptr;
		return "";
	}

	/**
	 *
	 * @param curr_sentence_node the first node of the sentences. \n
	 * Each neighbouring node must have 0-1 sentences below it for each language
	 * @param doc the current HTML document loaded
	 * @param sentences the sentence vector to fill
	 */
	void 汉英词典第三版Extractor::fill_sentences(
		lxb_dom_node*          curr_sentence_node,
		lxb_html_document_t*   doc,
		std::vector<Sentence>& sentences
	) {
		while (curr_sentence_node != nullptr) {
			Sentence curr_sentence{};

			curr_sentence.chinese = search_for_sentence(doc, curr_sentence_node, "hycd_3rd_zh");

			curr_sentence.english = search_for_sentence(doc, curr_sentence_node, "hycd_3rd_en");

			if (!curr_sentence.english.empty() || !curr_sentence.chinese.empty())
				sentences.push_back(curr_sentence);
			curr_sentence_node = lxb_dom_node_next(curr_sentence_node);
		}
	}

	std::vector<EntryInfo> 汉英词典第三版Extractor::extractMDictHTML(const std::string& html) {
		const std::u16string html_u16   = utf8::utf8to16(html);
		const auto           html_ucstr = reinterpret_cast<const lxb_char_t*>(html.c_str());
		lxb_html_document_t* doc        = lxb_html_parse(parser, html_ucstr, html.length());
		const auto           body       = lxb_dom_interface_node(doc->body);
		const auto           root       = body->first_child;

		std::vector<EntryInfo> res{};

		lxb_dom_node_t* pinyin_link = findChildClass(root, "hycd_3rd_pinyin")->first_child;
		while (pinyin_link != nullptr) {
			EntryInfo curr_entry{};

			auto link_node    = pinyin_link->first_child;
			curr_entry.pinyin = getRecursiveTextContent(link_node);

			auto              link_elem = lxb_dom_interface_element(link_node);
			std::size_t       href_len{};
			const std::string pinyin_find_tag = reinterpret_cast<const char*>(lxb_dom_element_get_attribute(
				link_elem,
				reinterpret_cast<const lxb_char_t*>("href"),
				4,
				&href_len
			));

			lxb_dom_node_t* part = findChildId(root, pinyin_find_tag.substr(1)); {
				lxb_dom_node_t*       citouci_search_root = part->first_child->first_child;
				lxb_dom_collection_t* citouci_col         = searchForClass(doc, citouci_search_root, "citouci");
				lxb_dom_node_t*       citouci             = lxb_dom_collection_node(citouci_col, 0);
				curr_entry.simp                           = getTextContent(citouci->first_child);
				trim(curr_entry.simp);
				lxb_dom_collection_destroy(citouci_col, true);
				citouci_col = nullptr;
			}

			lxb_dom_node_t* chixing_search_root = part->first_child->first_child->last_child->first_child->first_child->
			                                            first_child;
			lxb_dom_collection_t* chixing_col = searchForClass(doc, chixing_search_root, "chixing");

			if (lxb_dom_collection_length(chixing_col) == 0) {
				lxb_dom_node_t* fallback_chixing = chixing_search_root->first_child->next->first_child->first_child->
				                                                        first_child->first_child->first_child;
				std::string word_class = getTextContent(fallback_chixing->first_child);
				std::string definition = getRecursiveTextContent(fallback_chixing);
				Definition  curr_def{.word_class = word_class, .definition = definition};
			}

			for (size_t i = 0; i < lxb_dom_collection_length(chixing_col); i++) {
				lxb_dom_node_t* chixing = lxb_dom_collection_node(chixing_col, i);

				lxb_dom_node_t* def_search_root = chixing->parent->parent->parent->parent;

				lxb_dom_node_t* definition = def_search_root->next;
				if (!definition) {
					Definition curr_def{.word_class = getTextContent(chixing->first_child)};

					lxb_dom_node_t*   def_node       = chixing->parent;
					const std::string preprocess_def = getRecursiveTextContent(def_node);
					if (!preprocess_def.empty())
						curr_def.definition = process_definition(preprocess_def, curr_def.word_class);

					lxb_dom_node_t* sentence_root = def_search_root->first_child->first_child->next;
					fill_sentences(sentence_root, doc, curr_def.sentences);
					curr_entry.definitions.push_back(curr_def);
				}
				while (definition != nullptr) {
					Definition curr_def{.word_class = getTextContent(chixing->first_child)};

					lxb_dom_node_t*   def_node       = definition->first_child->first_child->first_child;
					const std::string preprocess_def = getRecursiveTextContent(def_node);
					curr_def.definition              = process_definition(preprocess_def, curr_def.word_class);

					lxb_dom_node_t* sentence_root = definition->first_child->first_child->next;
					fill_sentences(sentence_root, doc, curr_def.sentences);
					curr_entry.definitions.push_back(curr_def);
					definition = lxb_dom_node_next(definition);
				}
			}
			lxb_dom_collection_destroy(chixing_col, true);
			res.push_back(curr_entry);
			pinyin_link = lxb_dom_node_next(pinyin_link);
		}

		lxb_html_document_destroy(doc);
		lxb_html_parser_clean(parser);
		return res;
	}
}
