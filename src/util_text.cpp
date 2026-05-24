#include "util_text.h"


#include <utf8/cpp20.h>

#include <locale>


namespace iwra {
	std::string& ltrim(std::string& str) {
		const auto it2 = std::ranges::find_if(
			str,
			[](const char ch) {
				return !(std::isspace<char>(ch, std::locale::classic()) || ch == '\0');
			}
		);
		str.erase(str.begin(), it2);
		return str;
	}

	std::string& rtrim(std::string& str) {
		const auto it1 = std::find_if(
			str.rbegin(),
			str.rend(),
			[](const char ch) {
				return !(std::isspace<char>(ch, std::locale::classic()) || ch == '\0');
			}
		);
		str.erase(it1.base(), str.end());
		return str;
	}
}