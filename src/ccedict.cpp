#include "cccedict.h"

#include <ranges>
#include <regex>
#include <mio/mmap.hpp>
#include <spdlog/spdlog.h>
#include <utf8/cpp20.h>
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>

#include "util_utf8.h"

namespace iwra {
	bool CCCEdictDictParser::load(const std::filesystem::path& file_path) {
		indicators::show_console_cursor(false);
		indicators::ProgressBar bar{
			indicators::option::BarWidth{50},
			indicators::option::Start{"["},
			indicators::option::Fill{"■"},
			indicators::option::Lead{"■"},
			indicators::option::Remainder{" "},
			indicators::option::End{" ]"},
			indicators::option::PrefixText{"Loading Dictionary "},
			indicators::option::ForegroundColor{indicators::Color::yellow},
			indicators::option::ShowPercentage{true},
			indicators::option::ShowElapsedTime{true},
			indicators::option::ShowRemainingTime{true},
			indicators::option::FontStyles{std::vector{indicators::FontStyle::bold}}
		};


		std::vector<std::string_view> lines;
		mio::mmap_source              mmap(file_path.string());
		auto                          start = mmap.begin();
		auto                          it    = std::ranges::find(mmap, '\r');
		const auto                    end   = mmap.end();
		while (it != end) {
			if (*start != '#') {
				if (std::string_view line = {start, it};
					!line.empty()) {
					lines.emplace_back(line);
				}
			}
			start = it + 2;
			it    = std::find(start, end, '\r');
		}
		const std::size_t  min_length = lines.size();
		std::vector<Entry> parsed(min_length);
		dictionary.reserve(min_length);

		std::atomic<float> total_completed = 0;
		#pragma omp parallel for
		for (int i = 0; i < lines.size(); ++i) {
			parsed[i] = parse(lines[i]).value();
			total_completed += 1;
			if (total_completed / min_length * 100 > bar.current()) {
				bar.tick();
			}
		}

		for (const auto& curr : parsed) {
			const std::string simp = curr.getSimp();
			const std::string trad = curr.getTrad();
			dictionary[simp].emplace_back(curr);
			if (simp != trad) {
				dictionary[trad].emplace_back(curr);
			}
		}

		for (auto& value : dictionary | std::views::values) {
			std::ranges::reverse(value);
		}
		bar.mark_as_completed();
		indicators::show_console_cursor(true);
		return true;
	}

	std::optional<DictionaryParser::Entry> CCCEdictDictParser::parse(const std::string_view& line) {
		Entry res{};

		const std::string::size_type end_trad_pos = line.find(' ');
		if (end_trad_pos == std::string::npos) {
			return std::nullopt;
		}
		std::string_view trad = line.substr(0, end_trad_pos);

		const std::string::size_type end_simp_pos = line.find(' ', end_trad_pos + 1);
		if (end_simp_pos == std::string::npos) {
			return std::nullopt;
		}
		std::string_view simp = line.substr(end_trad_pos + 1, end_simp_pos - end_trad_pos - 1);

		const std::string::size_type sb = line.find('[');
		if (sb == std::string::npos) {
			return std::nullopt;
		}
		const bool                   is_v2_syntax     = line[sb + 1] == '[';
		const std::string::size_type start_pinyin_pos = sb + (is_v2_syntax ? 2 : 1);

		const std::string::size_type end_pinyin_pos = (is_v2_syntax)
		                                              ? line.find("]]", start_pinyin_pos)
		                                              : line.find(']', start_pinyin_pos);
		if (end_pinyin_pos == std::string::npos) {
			return std::nullopt;
		}
		const std::string_view pinyin = line.substr(start_pinyin_pos, end_pinyin_pos - start_pinyin_pos);

		const auto pinyin_split = split_pinyin(pinyin, is_v2_syntax);
		auto       simp_it      = simp.begin();
		const auto simp_end     = simp.end();
		auto       trad_it      = trad.begin();
		const auto trad_end     = trad.end();
		for (auto& curr_split : pinyin_split) {
			Word curr_word{};
			for (std::size_t i = 0; i < curr_split.size(); ++i) {
				const std::string& curr      = curr_split[i];
				char32_t           curr_simp = utf8::next(simp_it, simp_end);
				char32_t           curr_trad = utf8::next(trad_it, trad_end);

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
						toUtf8(curr_simp),
						toUtf8(curr_trad),
						pinyinNumberToTone(curr)
					);
				} else {
					const std::size_t curr_len = curr.length();
					std::string       simp_tot = toUtf8(curr_simp);
					std::string       trad_tot = toUtf8(curr_trad);
					for (; i < std::min(i + curr_len - 1, curr_split.size()); ++i) {
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
			res.words.emplace_back(curr_word);
		}

		std::string::size_type curr_pos;
		std::string::size_type prev_pos = end_pinyin_pos + (is_v2_syntax ? 4 : 3);
		while ((curr_pos = line.find('/', prev_pos)) != std::string::npos) {
			res.definitions.emplace_back(line.substr(prev_pos, curr_pos - prev_pos));
			prev_pos = curr_pos + 1;
		}

		return res;
	}

	std::vector<DictionaryParser::Entry> CCCEdictDictParser::getEntry(const std::string& hanzi) {
		if (const auto pos = dictionary.find(hanzi);
			pos != dictionary.end()) {
			return pos->second;
		}
		return {};
	}

	bool CCCEdictDictParser::isPinyinSingleWord(const std::string_view& in) {
		if (in.back() - U'0' < 1 || in.back() - U'0' > 5) {
			return false;
		}
		if (in.size() == 1) {
			return false;
		}

		auto       it  = in.begin();
		const auto end = in.end();

		bool has_vowel      = false;
		bool vowel_finished = false;
		while (it != end) {
			if (isVowel(utf8::next(it, end))) {
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

	bool CCCEdictDictParser::isPinyin(const std::string_view& in) {
		std::size_t start = 0;
		std::size_t end   = in.find_first_of("12345", start);

		if (end == std::string_view::npos || end == 0) {
			return false;
		}

		while (start != in.size()) {
			if (!isPinyinSingleWord(in.substr(start, end - start + 1))) {
				return false;
			}
			start = end + 1;
			end   = in.find_first_of("12345", start);
		}
		return true;
	}

	std::string CCCEdictDictParser::pinyinNumberToTone(const std::string& in_pinyin) {
		std::string out;
		std::size_t start = 0;
		std::size_t end   = in_pinyin.find_first_of(" 12345", start);
		for (bool has_extra_life = true; has_extra_life;
		     start               = end + 1, end = in_pinyin.find_first_of(" 12345", start)) {
			if (end == std::string::npos) {
				const std::size_t n = in_pinyin.length() - 1;
				if (const int number = in_pinyin[n] - '0';
					1 <= number && number <= 5 && in_pinyin[n] != ' ') {
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
				"ā",
				"á",
				"ǎ",
				"à",
				"a",
				"ē",
				"é",
				"ĕ",
				"è",
				"e",
				"ī",
				"í",
				"ǐ",
				"ì",
				"i",
				"ō",
				"ó",
				"ǒ",
				"ò",
				"o",
				"ū",
				"ú",
				"ǔ",
				"ù",
				"u",
				"ǖ",
				"ǘ",
				"ǚ",
				"ǜ",
				"ü",
				"Ā",
				"Á",
				"Ǎ",
				"À",
				"A",
				"Ē",
				"É",
				"Ě",
				"È",
				"E",
				"Ī",
				"Í",
				"Ǐ",
				"Ì",
				"I",
				"Ō",
				"Ó",
				"Ǒ",
				"Ò",
				"O",
				"Ū",
				"Ú",
				"Ǔ",
				"Ù",
				"U",
				"Ǖ",
				"Ǘ",
				"Ǚ",
				"Ǜ",
				"Ü",
			};
			if (const auto it0 = curr.find('a');
				it0 != std::string::npos) {
				curr.replace(it0, 1, tone_lut[number]);
			} else if (const auto it1 = curr.find('A');
				it1 != std::string::npos) {
				curr.replace(it1, 1, tone_lut[number + 30]);
			} else if (const auto it2 = curr.find('e');
				it2 != std::string::npos) {
				curr.replace(it2, 1, tone_lut[number + 5]);
			} else if (const auto it3 = curr.find('E');
				it3 != std::string::npos) {
				curr.replace(it3, 1, tone_lut[number + 35]);
			} else if (const auto it4 = curr.find("ou");
				it4 != std::string::npos) {
				curr.replace(it4, 1, tone_lut[number + 15]);
			} else if (const auto it5 = curr.find("Ou");
				it5 != std::string::npos) {
				curr.replace(it5, 1, tone_lut[number + 45]);
			} else if (const auto it6 = curr.find_last_of("aeiou:");
				it6 != std::string::npos) {
				const char32_t    letter = curr[it6];
				const std::size_t offset =
						(letter == 'a')
						? 0
						: (letter == 'e')
						  ? 5
						  : (letter == 'i')
						    ? 10
						    : (letter == 'o')
						      ? 15
						      : (letter == 'u')
						        ? 20
						        : (letter == ':')
						          ? (curr[it6] == 'u' ? 25 : 55)
						          : (letter == 'A')
						            ? 30
						            : (letter == 'E')
						              ? 35
						              : (letter == 'I')
						                ? 40
						                : (letter == 'O')
						                  ? 45
						                  : (letter == 'U')
						                    ? 50
						                    : 10000000000000;

				const std::size_t v_offset = (letter == ':') ? 1 : 0;
				curr.replace(it6 - v_offset, 1 + v_offset, tone_lut[number + offset]);
			}
			out += curr + ' ';
		}
		out.pop_back();
		return out;
	}

	std::vector<std::vector<std::string> > CCCEdictDictParser::split_pinyin(
		const std::string_view& pinyin,
		const bool              is_v2_syntax
	) {
		std::vector<std::vector<std::string> > res;
		std::vector<std::string>               curr_word;
		std::string                            carry;

		auto       it  = pinyin.begin();
		const auto end = pinyin.end();

		enum State { Normal, Combined, Connected };

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
};
