#include "util_pinyin.h"


#include <utf8/cpp20.h>

#include <array>


bool iwra::isPinyin(const std::string& in) {
	if (in.back() - U'0' < 1 || in.back() - U'0' > 5) { return false; }

	auto       it  = in.begin();
	const auto end = in.end();

	bool has_vowel      = false;
	bool vowel_finished = false;
	while (it != end) {
		const char32_t c = utf8::next(it, end);
		if (c == U'a' || c == U'e' || c == U'i' || c == U'o' || c == U'u' || c == U':') {
			if (vowel_finished) {
				return false;
			}
			has_vowel = true;
		} else if (has_vowel) {
			vowel_finished = true;
		}
	}
	return true;
}

bool iwra::isTone(const char c) {
	return c >= '1' && c <= '5';
}

bool iwra::isTone(const char32_t c) {
	return c >= U'1' && c <= U'5';
}

std::string iwra::pinyinNumberToTone(const std::string& in_pinyin) {
	std::string out;
	std::size_t start = 0;
	std::size_t end   = in_pinyin.find_first_of(" 12345", start);
	for (bool has_extra_life = true; has_extra_life; start = end + 1, end = in_pinyin.find_first_of(" 12345", start)) {
		if (end == std::string::npos) {
			const std::size_t n      = in_pinyin.length() - 1;
			const int         number = in_pinyin[n] - '0';
			if (1 <= number && number <= 5 && in_pinyin[n] != ' ') {
				break;
			}
			has_extra_life = false;
			end            = n;
		}

		if (in_pinyin[end] == ' ') {
			end -= 1;
		}
		std::string curr   = in_pinyin.substr(start, end - start + 1);
		const int   number = in_pinyin[end] - '0' - 1;
		if (0 > number || number > 4) {
			out += curr + ' ';
			continue;
		}
		curr.pop_back();

		constexpr std::array tone_lut = {
			"ā","á","ǎ","à","a",
			"ē","é","ĕ","è","e",
			"ī","í","ǐ","ì","i",
			"ō","ó","ǒ","ò","o",
			"ū","ú","ǔ","ù","",
			"ǖ","ǘ","ǚ","ǜ","ü"
		};
		if (const auto it0 = curr.find('a');
			it0 != std::string::npos) {
			curr.replace(it0, 1, tone_lut[number]);
		}
		else if (const auto it1 = curr.find('e');
			it1 != std::string::npos) {
			curr.replace(it1, 1, tone_lut[number + 5]);
		}
		else if (const auto it2 = curr.find("ou");
			it2 != std::string::npos) {
			curr.replace(it2, 1, tone_lut[number + 15]);
		}
		else if (const auto it3 = curr.find_last_of("aeiou:");
			it3 != std::string::npos) {
			const char32_t    letter = curr[it3];
			const std::size_t offset =
					(letter == 'a') ?  0 :
					(letter == 'e') ?  5 :
					(letter == 'i') ? 10 :
					(letter == 'o') ? 15 :
					(letter == 'u') ? 20 :
					25 ;

			const std::size_t v_offset = (letter == ':') ? 1 : 0;
			curr.replace(it3 - v_offset, 1 + v_offset, tone_lut[number + offset]);
		}
		out += curr + ' ';
	}
	out.pop_back();
	return out;
}
