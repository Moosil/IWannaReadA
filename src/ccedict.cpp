#include "dict_parser.h"


#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>
#include <mio/mmap.hpp>

#include <ranges>
#include <regex>

#include "log.h"
#include "util_pinyin.h"
#include "util_utf8.h"


namespace iwra {
	std::vector<std::vector<std::string>> split_pinyin(const std::string_view& pinyin, const bool is_v2_syntax) {
		std::vector<std::vector<std::string>> res;
		std::vector<std::string> curr_word;
		std::string carry;

		auto it = pinyin.begin();
		const auto end = pinyin.end();

		enum State {Normal, Combined, Connected};

		State st = Normal;

		while (it != end) {
			const char32_t c = utf8::next(it, end);
			if (c == U' ') {
				st = Normal;
				if (!carry.empty()) {
					curr_word.emplace_back(carry);
					carry.clear();
				}

				res.emplace_back(curr_word);
				curr_word.clear();
				continue;
			}
			switch (st) {
				case Normal: {
					if (c == U'{') {
						st = Combined;
						break;
					}
					if (is_v2_syntax) {
						if (isTone(c)) {
							curr_word.emplace_back(carry + toUtf8(c));
							carry.clear();
							break;
						}
						if (c == U'-') {
							if (!carry.empty()) {
								curr_word.emplace_back(carry);
								carry.clear();
							}
							st = Connected;
							break;
						}
					}
					carry += toUtf8(c);
					break;
				}
				case Combined: {
					if (c == U'}') {
						st = Normal;
						break;
					}
					carry += toUtf8(c);
					break;
				}
				case Connected: {
					if (c == U'-') {
						if (!carry.empty()) {
							curr_word.emplace_back(carry);
							carry.clear();
						}
						break;
					}
					carry += toUtf8(c);
					break;
				}
			}
		}
		if (!carry.empty()) {
			curr_word.emplace_back(carry);
		}
		if (!curr_word.empty()) {
			res.emplace_back(curr_word);
		}
		return res;
	}

	bool DictParser::load(const std::filesystem::path& file_path) {
		startTimeFunction();
		std::vector<std::string_view> lines;
		mio::mmap_source mmap(file_path.string());
		auto start = mmap.begin();
		auto it = std::ranges::find(mmap, '\r');
		const auto end = mmap.end();
		while (it != end) {
			if (*start != '#') {
				if (std::string_view line = {start, it}; !line.empty()) {
					lines.emplace_back(line);
				}
			}
			start = it + 2;
			it = std::find(start, end, '\r');
		}
		const std::size_t min_length = lines.size();
		std::vector<entry> parsed(min_length);
		dictionary.reserve(min_length);

		#pragma omp parallel for
		for (int i = 0; i < lines.size(); i++) {
			parsed[i] = parse(lines[i]).value();
		}

		for (const auto& curr : parsed) {
			const std::string simp = curr.get_simp();
			const std::string trad = curr.get_trad();
			dictionary[simp].emplace_back(curr);
			if (simp != trad) {
				dictionary[trad].emplace_back(curr);
			}
		}

		for (auto& value : dictionary | std::views::values) {
			std::ranges::reverse(value);
		}
		endTimeFunction();
		return true;
	}

	std::optional<DictParser::entry> DictParser::parse(const std::string_view& line) {
		entry            res{};

		const std::string::size_type end_trad_pos = line.find(' ');
		if (end_trad_pos == std::string::npos) { return std::nullopt; }
		std::string_view trad = line.substr(0, end_trad_pos);

		const std::string::size_type end_simp_pos = line.find(' ', end_trad_pos+1);
		if (end_simp_pos == std::string::npos) { return std::nullopt; }
		std::string_view simp = line.substr(end_trad_pos + 1, end_simp_pos - end_trad_pos - 1);

		const std::string::size_type sb = line.find('[');
		if (sb == std::string::npos) { return std::nullopt; }
		const bool is_v2_syntax = line[sb+1] == '[';
		const std::string::size_type start_pinyin_pos = sb + (is_v2_syntax ? 2 : 1);

		const std::string::size_type end_pinyin_pos = (is_v2_syntax) ? line.find("]]", start_pinyin_pos) : line.find(']', start_pinyin_pos);
		if (end_pinyin_pos == std::string::npos) { return std::nullopt; }
		const std::string_view pinyin = line.substr(start_pinyin_pos, end_pinyin_pos - start_pinyin_pos);

		const auto pinyin_split = split_pinyin(pinyin, is_v2_syntax);
		auto simp_it = simp.begin();
		const auto simp_end = simp.end();
		auto trad_it = trad.begin();
		const auto trad_end = trad.end();
		for (auto& curr_split : pinyin_split) {
			word curr_word{};
			for (std::size_t i = 0; i < curr_split.size(); i++) {
				const std::string& curr = curr_split[i];
				char32_t curr_simp = utf8::next(simp_it, simp_end);
				char32_t curr_trad = utf8::next(trad_it, trad_end);

				if (curr_simp == U'{' || curr_simp == U'}') {
					curr_simp = utf8::next(simp_it, simp_end);
					curr_trad = utf8::next(trad_it, trad_end);
				}

				// punctuation check
				if (curr_simp == U'·') {
					curr_word.characters.emplace_back(
						"·",
						"·",
						"·"
					);
					if (curr == "·") {
						continue;
					}
					curr_simp = utf8::next(simp_it, simp_end);
					curr_trad = utf8::next(trad_it, trad_end);
				}

				if (curr_simp == U'{' || curr_simp == U'}') {
					curr_simp = utf8::next(simp_it, simp_end);
					curr_trad = utf8::next(trad_it, trad_end);
				}

				if (isPinyin(curr)) {
					curr_word.characters.emplace_back(
						toUtf8(curr_trad),
						toUtf8(curr_simp),
						pinyinNumberToTone(curr)
					);
				} else {
					const std::size_t curr_len = curr.length();
					std::string simp_tot = toUtf8(curr_simp);
					std::string trad_tot = toUtf8(curr_trad);
					for (; i < std::min(i + curr_len - 1, curr_split.size()); i++) {
						simp_tot += toUtf8(utf8::next(simp_it, simp_end));
						trad_tot += toUtf8(utf8::next(trad_it, trad_end));
					}

					curr_word.characters.emplace_back(
						simp_tot,
						trad_tot,
						curr
					);
				}
			}
			res.word.emplace_back(curr_word);
		}

		// spdlog::info("{}/{} [{}]", res.get_simp(), res.get_trad(), res.get_pinyin());

		std::string::size_type curr_pos;
		std::string::size_type prev_pos = end_pinyin_pos+3;
		while ((curr_pos = line.find('/', prev_pos)) != std::string::npos) {
			res.definitions.emplace_back(line.substr(prev_pos, curr_pos - prev_pos));
			prev_pos = curr_pos+1;
		}

		return res;
	}

	std::vector<DictParser::entry> DictParser::get_entry(const std::string& hanzi) {
		if (const auto pos = dictionary.find(hanzi); pos != dictionary.end()) {
			return pos->second;
		}
		return {};
	}
};