#pragma once


#include <string>


namespace iwra {
	bool isPinyin(const std::string_view& in);

	bool _isPinyin(const std::string_view& in);

	bool isTone(const std::string_view& c);

	bool isTone(char c);

	bool isTone(char32_t c);

	std::string pinyinNumberToTone(const std::string& in_pinyin);
}
