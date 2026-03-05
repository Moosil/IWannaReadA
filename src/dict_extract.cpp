#include "dict_extract.h"


#include <stdexcept>
#include <spdlog/spdlog.h>

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
			const std::string child_class_string = reinterpret_cast<const char*>(child_class->value->data);
			if (child_class_string == class_name) {
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

	std::string DictExtractor::getTextContent(lxb_dom_node_t* node) {
		if (lxb_dom_node_tag_id(node) == LXB_TAG__TEXT) {
			const auto* cdata = lxb_dom_interface_character_data(node);
			return reinterpret_cast<const char*>(cdata->data.data);
		}
		return "";
	}

	std::string DictExtractor::getRecursiveTextContent(lxb_dom_node_t* node) {
		auto child = node->first_child;
		if (child == nullptr) {
			return getTextContent(node);
		}

		std::string text_content{};
		while (child != nullptr) {
			text_content += getRecursiveTextContent(child);
			child        = lxb_dom_node_next(child);
		}
		return text_content;
	}
}
