#include "util_utf8.h"


namespace iwra {
	auto utf8Find(
		const std::string& in,
		const std::string& to_find
	) -> std::pair<std::string::const_iterator, std::string::const_iterator> {
		auto           it        = in.begin();
		auto           prev      = in.begin();
		const auto     end       = in.end();
		const char32_t find_char = utf8::peek_next(to_find.begin(), to_find.end());
		while (it != end) {
			if (utf8::next(it, end) == find_char) {
				if (std::string(prev, end).starts_with(to_find)) {
					return std::make_pair(prev, prev + static_cast<std::string::difference_type>(to_find.size()));
				}
			}
			prev = it;
		}
		return std::make_pair(end, end);
	}

	std::string::const_iterator utf8Find(const std::string& in, const char32_t to_find) {
		auto       it  = in.begin();
		const auto end = in.end();
		while (it != end) {
			if (utf8::next(it, end) == to_find) {
				return it;
			}
		}
		return end;
	}

	std::size_t utf8Length(const std::string& in) {
		std::size_t res = 0;
		auto        it  = in.begin();
		const auto  end = in.end();
		while (it != end) {
			utf8::next(it, end);
			res++;
		}
		return res;
	}
}
