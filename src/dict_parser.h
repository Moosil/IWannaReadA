#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>


namespace iwra {
	class DictParser {
	public:
		virtual ~DictParser() = default;

		struct character {
			std::string simp;
			std::string trad;
			std::string pinyin;
		};

		struct word {
			std::vector<character> characters;

			[[nodiscard]] std::string get_simp() const {
				std::string simp;
				for (const auto& character : characters) {
					simp += character.simp;
				}
				return simp;
			}

			[[nodiscard]] std::string get_trad() const {
				std::string trad;
				for (const auto& character : characters) {
					trad += character.trad;
				}
				return trad;
			}

			[[nodiscard]] std::string get_pinyin() const {
				std::string pinyin;
				for (const auto& character : characters) {
					pinyin += character.pinyin;
				}
				return pinyin;
			}
		};

		struct entry {
			std::vector<word>        words;
			std::vector<std::string> definitions;

			[[nodiscard]] std::string get_simp() const {
				std::string simp;
				for (const auto& w : words) {
					simp += w.get_simp();
				}
				return simp;
			}

			[[nodiscard]] std::string get_trad() const {
				std::string trad;
				for (const auto& w : words) {
					trad += w.get_trad();
				}
				return trad;
			}

			[[nodiscard]] std::string get_pinyin() const {
				std::string pinyin;
				for (const auto& w : words) {
					pinyin += w.get_pinyin() + ' ';
				}
				pinyin.pop_back();
				return pinyin;
			}
		};

		virtual bool load(const std::filesystem::path& file_path);

		static std::optional<entry> parse(const std::string_view& line);

		virtual std::vector<entry> get_entry(const std::string& hanzi);

		std::unordered_map<std::string, std::vector<entry> > dictionary;
	};
}
