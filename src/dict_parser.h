#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace iwra {
	class DictionaryParser {
	public:
		virtual ~DictionaryParser() = default;

		struct Character {
			std::string simp;
			std::string trad;
			std::string pinyin;
		};

		struct Word {
			std::vector<Character> characters;

			[[nodiscard]] std::string getSimp() const {
				std::string simp;
				for (const auto& character : characters) {
					simp += character.simp;
				}
				return simp;
			}

			[[nodiscard]] std::string getTrad() const {
				std::string trad;
				for (const auto& character : characters) {
					trad += character.trad;
				}
				return trad;
			}

			[[nodiscard]] std::string getPinyin() const;
		};

		struct Entry {
			std::vector<Word>        words;
			std::vector<std::string> definitions;

			[[nodiscard]] std::string getSimp() const {
				std::string simp;
				for (const auto& w : words) {
					simp += w.getSimp();
				}
				return simp;
			}

			[[nodiscard]] std::string getTrad() const {
				std::string trad;
				for (const auto& w : words) {
					trad += w.getTrad();
				}
				return trad;
			}

			[[nodiscard]] std::string getPinyin() const {
				std::string pinyin;
				for (const auto& w : words) {
					pinyin += w.getPinyin() + ' ';
				}
				pinyin.pop_back();
				return pinyin;
			}
		};

		virtual bool load(const std::filesystem::path& file_path) = 0;

		virtual std::vector<Entry> getEntry(const std::string& hanzi) = 0;

		std::unordered_map<std::string, std::vector<Entry> > dictionary;
	};

	inline std::string DictionaryParser::Word::getPinyin() const {
		std::string pinyin;
		for (const auto& character : characters) {
			pinyin += character.pinyin;
		}
		return pinyin;
	}
}
