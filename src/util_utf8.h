#pragma once

#include <utf8/cpp20.h>

#include <string>


namespace iwra {
	std::string::const_iterator utf8Find(const std::string& in, char32_t to_find);

	std::string toUtf8(char32_t c);

	bool isAlphanum(char32_t c);
}
