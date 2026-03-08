#include "dict_extract.h"


#include <stack>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>

#include "util.h"
#include "lexbor/dom/dom.h"

namespace ocr {
	DictExtractor::DictExtractor() {
		parser = lxb_html_parser_create();
		if (const lxb_status_t status = lxb_html_parser_init(parser); status != LXB_STATUS_OK) {
			spdlog::error("could not initialise HTML parser: {}", status);
			throw std::runtime_error(std::format("could not initialise HTML parser: {}", status));
		}
	}

	DictExtractor::~DictExtractor() {
		lxb_html_parser_destroy(parser);
	}

	lxb_dom_collection_t* DictExtractor::searchForClass(
		lxb_html_document_t* doc,
		lxb_dom_node_t*      root,
		const std::string&   class_name
	) {
		lxb_dom_collection_t* col = lxb_dom_collection_make(&doc->dom_document, 16);

		const auto search_ucstr = reinterpret_cast<const lxb_char_t*>(class_name.c_str());
		lxb_dom_elements_by_class_name(
			lxb_dom_interface_element(root),
			col,
			search_ucstr,
			class_name.length()
		);
		// need to call lxb_dom_collection_destroy(col, true);
		return col;
	}

	lxb_dom_node_t* DictExtractor::findChildClass(lxb_dom_node* parent, const std::string& class_name) {
		lxb_dom_node_t* find_child = lxb_dom_node_first_child(parent);
		while (find_child != nullptr) {
			const auto child_elem  = lxb_dom_interface_element(find_child);
			const auto child_class = child_elem->attr_class;
			if (!child_class) {
				find_child = lxb_dom_node_next(find_child);
				continue;
			}
			if (const std::string child_class_string = reinterpret_cast<const char*>(child_class->value->data);
				child_class_string == class_name) {
				break;
			}

			find_child = lxb_dom_node_next(find_child);
		}
		return find_child;
	}

	lxb_dom_node_t* DictExtractor::findChildId(lxb_dom_node* parent, const std::string& id_name) {
		lxb_dom_node_t* find_child = lxb_dom_node_first_child(parent);
		while (find_child != nullptr) {
			const auto child_elem = lxb_dom_interface_element(find_child);
			const auto child_id   = child_elem->attr_id;
			if (!child_id) {
				find_child = lxb_dom_node_next(find_child);
				continue;
			}
			const std::string child_id_string = reinterpret_cast<const char*>(child_id->value->data);
			if (child_id_string == id_name) {
				break;
			}

			find_child = lxb_dom_node_next(find_child);
		}
		return find_child;
	}

	std::string DictExtractor::getTextContent(lxb_dom_node_t* node, ExtractTextHelper* helper) {
		if (lxb_dom_node_tag_id(node) == LXB_TAG__TEXT) {
			const auto* cdata = lxb_dom_interface_character_data(node);
			std::string inner_text = reinterpret_cast<const char*>(cdata->data.data);
			if (const auto parent_class = lxb_dom_interface_element(node->parent)->attr_class) {
				if (const std::string parent_class_string = reinterpret_cast<const char*>(parent_class->value->data);
					parent_class_string == "pinyin") {
					if (inner_text.find_first_not_of("āēīōūǖĀĒĪŌŪǕáéíóúǘÁÉÍÓÚǗǎěǐǒǔǚǍĚǏǑǓǙăĕĭŏŭĂĔĬŎŬàèìòùǜÀÈÌÒÙǛaeiouüAEIOUÜ bcdfghjklmnpqrstvwxyzBCDFGHJKLMNPQRSTVWXYZ") == std::string::npos) {
						if (helper && helper->curr.size() != 0 && helper->curr[helper->curr.size() - 1] == u'(') helper->curr.erase(helper->curr.size() - 1);
						if (helper) helper->remove_prefix_buffer = 2;
						return ""; //inner text is the pinyin
					}
					spdlog::info("discarded {} of node because parent.class was \"pinyin\" and it didn't contain only pinyin characters", inner_text);
					return "";
				}
			}
			return inner_text;
		}
		if (lxb_dom_node_tag_id(node) == LXB_TAG_IMG || lxb_dom_node_tag_id(node) == LXB_TAG_IMAGE) {
			if (const std::string child_attr_string = reinterpret_cast<const char*>(lxb_dom_interface_element(node)->first_attr->value->data);
				child_attr_string == "file://imgs/hand.gif") {
				return "[linktoreference]";
			}
		}
		return "";
	}

	std::vector<std::string> DictExtractor::getRecursiveTextContent(lxb_dom_node_t* node) {
		ExtractTextHelper helper{};

		std::stack<lxb_dom_node_t*> stack;
		stack.push(node);
		spdlog::info("started new");
		while (!stack.empty()) {
			lxb_dom_node_t* curr = stack.top(); stack.pop();
			for (auto* child = curr->last_child; child != nullptr; child = child->prev) {
				stack.push(child);
			}
			if (curr->first_child == nullptr) {
				const std::size_t remove_prefix_buffer = helper.remove_prefix_buffer;
				helper.remove_prefix_buffer = 0;
				std::string curr_contents = getTextContent(curr, &helper);
				if (std::u16string curr_cont_u16 = utf8::utf8to16(curr_contents);
				curr_contents == "[linktoreference]") {
					std::string curr_utf8 = utf8::utf16to8(helper.curr);
					helper.result.push_back(curr_utf8);
					spdlog::info("appended {} to the result", curr_utf8);
					helper.curr = u"";
				} else {
					if (remove_prefix_buffer > 0) {
						spdlog::info("removed {} from start of {}", helper.remove_prefix_buffer, curr_contents);
						curr_cont_u16.erase(0, remove_prefix_buffer);
					}
					std::string curr_utf8 = utf8::utf16to8(helper.curr);
					spdlog::info("added \"{}\" to curr:\"{}\"", curr_contents, curr_utf8);
					helper.curr += curr_cont_u16;
				}
			}
		}
		if (!helper.curr.empty()) {
		const std::string curr_utf8 = utf8::utf16to8(helper.curr);
			helper.result.push_back(curr_utf8);
					spdlog::info("appended {} to the result", curr_utf8);
		}

		spdlog::info("finished");
		return helper.result;
	}
}
