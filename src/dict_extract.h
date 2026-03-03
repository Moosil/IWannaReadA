#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <lexbor/html/html.h>


namespace ocr {
	struct Sentence {
		std::string chinese;
		std::string english;
	};

	struct Definition {
		std::string           word_class;
		std::string           definition;
		std::vector<Sentence> sentences{};
	};

	struct EntryInfo {
		std::string              simp;
		std::string              trad;
		std::string pinyin;
		std::vector<Definition>  definitions{};
	};

	class DictExtractor {
	public:
		DictExtractor();

		virtual ~DictExtractor();

		static lxb_dom_collection_t* searchForClass(
			lxb_html_document_t* doc,
			lxb_dom_node_t*      root,
			const std::string&   class_name
		);

		static lxb_dom_node_t* findChildClass(lxb_dom_node* parent, const std::string& class_name);

		static lxb_dom_node_t* findChildId(lxb_dom_node* parent, const std::string& id_name);

		static std::string getTextContent(lxb_dom_node_t* node);

		static std::string getRecursiveTextContent(lxb_dom_node_t* node);

		[[nodiscard]] virtual std::vector<EntryInfo> extractMDictHTML(const std::string& html) const = 0;

	protected:
		lxb_html_parser_t* parser;
	};
}
