#include "dict_extract_汉英词典（第3版）.h"

#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>

namespace ocr {
	std::vector<EntryInfo> 汉英词典第三版Extractor::extractMDictHTML(const std::string& html) const {
		const std::u16string html_u16   = utf8::utf8to16(html);
		const auto           html_ucstr = reinterpret_cast<const lxb_char_t*>(html.c_str());
		lxb_html_document_t* doc        = lxb_html_parse(parser, html_ucstr, html.length());
		const auto           body       = lxb_dom_interface_node(doc->body);
		const auto           root       = body->first_child;

		std::vector<EntryInfo> res{};

		lxb_dom_node_t* pinyin_child = findChildClass(root, "hycd_3rd_pinyin");
		lxb_dom_node_t* pinyin_link  = lxb_dom_node_first_child(pinyin_child);
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

			lxb_dom_node_t* part = findChildId(root, pinyin_find_tag.substr(1));

			lxb_dom_node_t*       citouci_search_root = part->first_child->first_child;
			lxb_dom_collection_t* citouci_col         = searchForClass(doc, citouci_search_root, "citouci");
			lxb_dom_node_t*       citouci             = lxb_dom_collection_node(citouci_col, 0);
			curr_entry.simp                           = getTextContent(citouci->first_child);
			curr_entry.trad                           = "-";
			lxb_dom_collection_destroy(citouci_col, true);
			citouci_col = nullptr;

			lxb_dom_node_t* chixing_search_root = part->first_child->first_child->last_child->first_child->first_child->
														first_child;
			lxb_dom_collection_t* chixing_col = searchForClass(doc, chixing_search_root, "chixing");

			for (size_t i = 0; i < lxb_dom_collection_length(chixing_col); i++) {
				lxb_dom_node_t* chixing = lxb_dom_collection_node(chixing_col, i);

				lxb_dom_node_t* def_search_root = chixing->parent->parent->parent->parent;

				lxb_dom_node_t* definition = def_search_root->next;
				if (!definition) {
					Definition curr_def{.word_class = getTextContent(chixing->first_child)};

					const std::string preprocess_def = getRecursiveTextContent(chixing->parent);

					std::u16string       process_def     = utf8::utf8to16(preprocess_def);
					const std::u16string word_class_u16  = utf8::utf8to16(curr_def.word_class);
					const std::size_t    word_class_find = process_def.find(word_class_u16);
					if (word_class_find != std::string::npos) {
						process_def.replace(word_class_find, word_class_u16.length(), u"");
					}
					const std::size_t first_space    = process_def.find(u' ');
					const std::size_t next_non_space = process_def.find_first_not_of(u' ', first_space);
					curr_def.definition              = utf8::utf16to8(process_def.substr(next_non_space));

					lxb_dom_node_t* sentence = def_search_root->first_child->first_child->next;
					while (sentence != nullptr) {
						Sentence curr_sentence{};

						lxb_dom_collection_t* hycd_3rd_zh_col = searchForClass(doc, sentence, "hycd_3rd_zh");
						lxb_dom_node_t*       hycd_3rd_zh     = lxb_dom_collection_node(hycd_3rd_zh_col, 0);
						lxb_dom_collection_t* hycd_3rd_en_col = searchForClass(doc, sentence, "hycd_3rd_en");
						lxb_dom_node_t*       hycd_3rd_en     = lxb_dom_collection_node(hycd_3rd_en_col, 0);

						if (!hycd_3rd_zh || !hycd_3rd_en) {
							spdlog::error(
								"[DictExtract] malformed mdict: {}, {}",
								curr_def.word_class,
								curr_def.definition
							);
							lxb_dom_collection_destroy(hycd_3rd_zh_col, true);
							hycd_3rd_zh_col = nullptr;
							lxb_dom_collection_destroy(hycd_3rd_en_col, true);
							hycd_3rd_en_col = nullptr;
							sentence        = lxb_dom_node_next(sentence);
							continue;
						}

						std::string sentence_chinese = getTextContent(hycd_3rd_zh->first_child);
						std::string sentence_english = getTextContent(hycd_3rd_en->first_child);

						curr_sentence.chinese = sentence_chinese;
						curr_sentence.english = sentence_english;
						lxb_dom_collection_destroy(hycd_3rd_zh_col, true);
						hycd_3rd_zh_col = nullptr;
						lxb_dom_collection_destroy(hycd_3rd_en_col, true);
						hycd_3rd_en_col = nullptr;

						curr_def.sentences.push_back(curr_sentence);
						sentence = lxb_dom_node_next(sentence);
					}
					curr_entry.definitions.push_back(curr_def);
				}
				while (definition != nullptr) {
					Definition curr_def{.word_class = getTextContent(chixing->first_child)};

					lxb_dom_node_t* def_node       = definition->first_child->first_child->first_child;
					std::string     preprocess_def = getRecursiveTextContent(def_node);

					std::u16string       process_def     = utf8::utf8to16(preprocess_def);
					const std::u16string word_class_u16  = utf8::utf8to16(curr_def.word_class);
					const std::size_t    word_class_find = process_def.find(word_class_u16);
					if (word_class_find != std::string::npos) {
						process_def.replace(word_class_find, word_class_u16.length(), u"");
					}
					const std::size_t first_space    = process_def.find(u' ');
					const std::size_t next_non_space = process_def.find_first_not_of(u' ', first_space);
					curr_def.definition              = utf8::utf16to8(process_def.substr(next_non_space));

					lxb_dom_node_t* sentence = definition->first_child->first_child->next;
					while (sentence != nullptr) {
						Sentence curr_sentence{};

						lxb_dom_collection_t* hycd_3rd_zh_col = searchForClass(doc, sentence, "hycd_3rd_zh");
						lxb_dom_node_t*       hycd_3rd_zh     = lxb_dom_collection_node(hycd_3rd_zh_col, 0);
						lxb_dom_collection_t* hycd_3rd_en_col = searchForClass(doc, sentence, "hycd_3rd_en");
						lxb_dom_node_t*       hycd_3rd_en     = lxb_dom_collection_node(hycd_3rd_en_col, 0);

						if (!hycd_3rd_zh || !hycd_3rd_en) {
							spdlog::error(
								"[DictExtract] malformed mdict: {}, {}",
								curr_def.word_class,
								curr_def.definition
							);
							lxb_dom_collection_destroy(hycd_3rd_zh_col, true);
							hycd_3rd_zh_col = nullptr;
							lxb_dom_collection_destroy(hycd_3rd_en_col, true);
							hycd_3rd_en_col = nullptr;
							sentence        = lxb_dom_node_next(sentence);
							continue;
						}

						std::string sentence_chinese = getTextContent(hycd_3rd_zh->first_child);
						std::string sentence_english = getTextContent(hycd_3rd_en->first_child);

						lxb_dom_collection_destroy(hycd_3rd_zh_col, true);
						hycd_3rd_zh_col = nullptr;
						lxb_dom_collection_destroy(hycd_3rd_en_col, true);
						hycd_3rd_en_col = nullptr;

						curr_sentence.chinese = sentence_chinese;
						curr_sentence.english = sentence_english;

						curr_def.sentences.push_back(curr_sentence);
						sentence = lxb_dom_node_next(sentence);
					}
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