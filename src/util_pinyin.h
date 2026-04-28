#pragma once


#include <string>


namespace iwra {
	bool isPinyin(const std::string& in);

	bool isTone(char c);

	bool isTone(char32_t c);

	std::string pinyinNumberToTone(const std::string& in_pinyin);
}
