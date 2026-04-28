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

	std::string& trim(std::string& str) {
		return ltrim(rtrim(str));
	}

	std::string trim_copy(const std::string& str) {
		auto s = str;
		return ltrim(rtrim(s));
	}

	std::wstring utf8ToWide(const std::string& str) {
		const std::u16string u16str = utf8::utf8to16(str);
		const auto           wcstr  = reinterpret_cast<const wchar_t*>(u16str.c_str());
		return wcstr;
	}

	std::string wideToUtf8(const std::wstring& wstr) {
		const std::u16string u16str = reinterpret_cast<const char16_t*>(wstr.c_str());
		const auto           str    = utf8::utf16to8(u16str);
		return str;
	}
}