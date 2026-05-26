#pragma once

#include <utf8/cpp20.h>
#include <string>


namespace iwra {
	std::pair<std::string::const_iterator, std::string::const_iterator> utf8Find(
		const std::string& in,
		const std::string& to_find
	);

	std::string::const_iterator utf8Find(const std::string& in, char32_t to_find);

	inline std::string toUtf8(const char32_t c) {
		std::string out;
		utf8::append(c, std::back_inserter(out));
		return out;
	}

	inline bool isAlphanum(const char32_t c) {
		return (U'a' <= c && c <= U'z') || (U'A' <= c && c <= U'Z') || (U'0' <= c && c <= U'9');
	}

	std::size_t utf8Length(const std::string& in);
}
