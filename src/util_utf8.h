#pragma once

#include <utf8/cpp20.h>

#include <string>


namespace iwra {
	std::pair<std::string::const_iterator, std::string::const_iterator> utf8Find(const std::string& in, const std::string& to_find);

	std::string::const_iterator utf8Find(const std::string& in, char32_t to_find);

	std::string toUtf8(char32_t c);

	bool isAlphanum(char32_t c);

	std::size_t utf8Length(const std::string& in);
}
