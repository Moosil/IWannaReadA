#include "util_utf8.h"

std::string::const_iterator iwra::utf8Find(const std::string& in, const char32_t to_find) {
	auto       it  = in.begin();
	const auto end = in.end();
	while (it != end) {
		if (utf8::next(it, end) == to_find) {
			return it;
		}
	}
	return end;
}

std::string iwra::toUtf8(const char32_t c) {
	std::string out;
	utf8::append(c, std::back_inserter(out));
	return out;
}

bool iwra::isAlphanum(const char32_t c) {
	return (U'a' <= c && c <= U'z') || (U'A' <= c && c <= U'Z') || (U'0' <= c && c <= U'9');
}
