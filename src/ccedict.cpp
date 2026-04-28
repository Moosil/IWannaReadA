#include "dict_parser.h"

#include <ranges>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>
#include <regex>

#include "log.h"
#include "util_pinyin.h"
#include "util_utf8.h"

namespace iwra {
	std::vector<std::vector<std::string>> split_pinyin(const std::string& pinyin, const bool is_v2_syntax) {
		std::vector<std::vector<std::string>> res;
		std::vector<std::string> curr_word;
		std::string carry;

		auto it = pinyin.begin();
		const auto end = pinyin.end();

		bool is_combined = false;
		bool is_connected = false;

		while (it != end) {
			const char32_t c = utf8::next(it, end);
			if (c == U'{') {
				is_combined = true;
			} else if (c == U'}') {
				is_combined = false;
			} else if (c == U' ') {
				if (!carry.empty()) {
					if (!is_v2_syntax && isTone(carry.back())) {
						curr_word.emplace_back(carry);
						carry.clear();
					} else {
						if (is_connected) {
							curr_word.emplace_back(carry);
						} else {
							auto carry_it = carry.begin();
							const auto carry_end = carry.end();
							while (carry_it != carry_end) {
								curr_word.emplace_back(toUtf8(utf8::next(carry_it, carry_end)));
							}
						}
					}
					carry.clear();
				}

				res.emplace_back(curr_word);
				curr_word = {};
			} else if (is_v2_syntax && !is_combined && (isTone(c) || c == U'-')) {
				if (isTone(c)) {
					curr_word.emplace_back(carry + toUtf8(c));
				} else if (c == U'-') {
					if (!carry.empty() && is_connected) {
						curr_word.emplace_back(carry);
					}
					is_connected = true;
				}
				carry.clear();
			} else {
				carry += toUtf8(c);
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
		file.open(file_path, std::ios::binary);
		if (!file) {
			spdlog::error("ccedict file did not open at: {}", file_path.string());
			return false;
		}

		// Source - https://stackoverflow.com/a/3072840
		// Posted by Abhay
		// Retrieved 2026-04-22, License - CC BY-SA 2.5
		std::ifstream inFile("file");
		const auto entry_cnt = std::count(std::istreambuf_iterator(inFile),
				   std::istreambuf_iterator<char>(), '\n');
		dictionary.reserve(entry_cnt);

		std::string line;
		startTimeFunction();
		while (std::getline(file, line)) {
			if (!line.empty() && line[0] != '#') {
				entry curr = parse(line).value();
				const std::string simp = curr.get_simp();
				const std::string trad = curr.get_trad();
				if (const auto simp_it = dictionary.find(simp); simp_it != dictionary.end()) {
					simp_it->second.push_back(curr);
				} else {
					dictionary.insert({simp, {curr}});
				}
				if (simp != trad) {
					if (const auto trad_it = dictionary.find(trad); trad_it != dictionary.end()) {
						trad_it->second.push_back(curr);
					} else {
						dictionary.insert({trad, {curr}});
					}
				}
			}
		}
		for (auto& value : dictionary | std::views::values) {
			std::ranges::reverse(value);
		}
		endTimeFunction();
		return true;
	}

	std::optional<DictParser::entry> DictParser::parse(const std::string& line) {
		entry res{};
		std::string trad;
		std::string simp;
		std::string pinyin;

		const std::string::size_type end_trad_pos = line.find(' ');
		if (end_trad_pos == std::string::npos) { return std::nullopt; }
		trad.assign(line, 0, end_trad_pos);

		const std::string::size_type end_simp_pos = line.find(' ', end_trad_pos+1);
		if (end_simp_pos == std::string::npos) { return std::nullopt; }
		simp.assign(line, end_trad_pos+1, end_simp_pos - end_trad_pos-1);

		const std::string::size_type sb = line.find('[');
		if (sb == std::string::npos) { return std::nullopt; }
		const bool is_v2_syntax = line[sb+1] == '[';
		const std::string::size_type start_pinyin_pos = sb + (is_v2_syntax ? 2 : 1);

		const std::string::size_type end_pinyin_pos = (is_v2_syntax) ? line.find("]]", start_pinyin_pos) : line.find(']', start_pinyin_pos);
		if (end_pinyin_pos == std::string::npos) { return std::nullopt; }
		pinyin.assign(line, start_pinyin_pos, end_pinyin_pos - start_pinyin_pos);

		if (!is_v2_syntax) {
			std::erase(simp, ' ');
			std::erase(trad, ' ');
		}

		std::size_t hanzi_len = 0;
		if (is_v2_syntax) {
			bool is_combined = false;
			bool alphanum_chain = false;
			auto it = simp.begin();
			const auto end = simp.end();
			while (it != end) {
				const char32_t c = utf8::next(it, end);
				if (c == U'{') {
					is_combined = true;
					if (alphanum_chain) {
						alphanum_chain = false;
						hanzi_len++;
					}
				} else if (c == U'}') {
					is_combined = false;
					hanzi_len++;
				} else if (!is_combined && isAlphanum(c)) {
					alphanum_chain = true;
				} else if (!is_combined) {
					hanzi_len++;
					if (alphanum_chain && !isAlphanum(c)) {
						alphanum_chain = false;
						hanzi_len++;
					}
				}
			}
			if (alphanum_chain) {
				hanzi_len++;
			}
		} else {
			auto it = simp.begin();
			const auto end = simp.end();
			while (it != end) {
				utf8::next(it, end);
				hanzi_len++;
			}
		}


		auto pinyin_split = split_pinyin(pinyin, is_v2_syntax);
		std::size_t pinyin_len = 0;
		for (const auto& v : pinyin_split) pinyin_len += v.size();
		if (pinyin_len == hanzi_len) {
			auto simp_it = simp.begin();
			const auto simp_end = simp.end();
			auto trad_it = trad.begin();
			const auto trad_end = trad.end();
			for (const auto& v : pinyin_split) {
				word curr_word{};
				for (const auto& curr : v) {
					curr_word.characters.emplace_back(
						toUtf8(utf8::next(simp_it, simp_end)),
						toUtf8(utf8::next(trad_it, trad_end)),
						pinyinNumberToTone(curr)
					);
				}
				res.word.emplace_back(curr_word);
			}
		} else {
			auto simp_it = simp.begin();
			const auto simp_end = simp.end();
			auto trad_it = trad.begin();
			const auto trad_end = trad.end();
			for (auto& curr_split : pinyin_split) {
				word curr_word{};
				for (std::size_t i = 0; i < curr_split.size(); i++) {
					const std::string& curr = curr_split[i];
					const char32_t curr_simp = utf8::next(simp_it, simp_end);
					const char32_t curr_trad = utf8::next(trad_it, trad_end);

					// punctuation check
					if (curr_simp == U'·' && utf8Find(curr, U'·') != curr.end()) {
						curr_word.characters.emplace_back(
							"·",
							"·",
							"·"
						);
						utf8::next(simp_it, simp_end);
					}

					if (isPinyin(curr)) {
						curr_word.characters.emplace_back(
							toUtf8(curr_trad),
							toUtf8(curr_simp),
							pinyinNumberToTone(curr)
						);
					} else {
						std::size_t curr_len = curr.length();

						std::string simp_tot = toUtf8(curr_simp);
						std::string trad_tot = toUtf8(curr_trad);
						for (; i < std::min(i + curr_len, curr_split.size()); i++) {
							simp_tot += toUtf8(utf8::next(simp_it, simp_end));
							trad_tot += toUtf8(utf8::next(trad_it, trad_end));
						}

						curr_word.characters.emplace_back(
							simp_tot,
							trad_tot,
							pinyinNumberToTone(curr)
						);
					}
				}
				res.word.emplace_back(curr_word);
			}
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