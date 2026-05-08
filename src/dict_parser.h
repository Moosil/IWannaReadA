#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <ranges>
#include <variant>


namespace iwra {
	class DictParser {
	public:
		struct character {
			std::string              simp;
			std::string              trad;
			std::string              pinyin;
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
			std::vector<word> word;
			std::vector<std::string> definitions;

			[[nodiscard]] std::string get_simp() const {
				std::string simp;
				for (const auto& w : word) {
					simp += w.get_simp();
				}
				return simp;
			}

			[[nodiscard]] std::string get_trad() const {
				std::string trad;
				for (const auto& w : word) {
					trad += w.get_trad();
				}
				return trad;
			}

			[[nodiscard]] std::string get_pinyin() const {
				std::string pinyin;
				for (const auto& w : word) {
					pinyin += w.get_pinyin() + ' ';
				}
				pinyin.pop_back();
				return pinyin;
			}
		};

		bool load(const std::filesystem::path& file_path);

		static std::optional<entry> parse(const std::string_view& line);

		std::vector<entry> get_entry(const std::string& hanzi);

		std::unordered_map<std::string, std::vector<entry>> dictionary;
	};
}
