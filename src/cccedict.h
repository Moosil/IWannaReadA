#pragma once

#include "dict_parser.h"


namespace iwra {
	class CCCEdictDictParser : public DictParser {
	public:
		bool load(const std::filesystem::path& file_path) override;

		static std::optional<entry> parse(const std::string_view& line);

		std::vector<entry> get_entry(const std::string& hanzi) override;

	private:
		static bool _isPinyin(const std::string_view& in);

		static bool isPinyin(const std::string_view& in);

		static bool isTone(const char c) {
			return c >= '1' && c <= '5';
		}

		static bool isTone(const char32_t c) {
			return c >= U'1' && c <= U'5';
		}

		static bool isVowel(const char32_t c) {
			return c == U'a' || c == U'e' || c == U'i' || c == U'o' || c == U'u' || c == U':' || c == U'A' || c == U'E'
			       || c == U'I' || c == U'O' || c == U'U';
		}

		static std::string pinyinNumberToTone(const std::string& in_pinyin);

		static std::vector<std::vector<std::string> > split_pinyin(
			const std::string_view& pinyin,
			const bool              is_v2_syntax
		);
	};
}
