#pragma once

#include <complex.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace iwra {
	class Config {
	public:
		static constexpr int defaultWidth                 = 256;
		static constexpr int defaultHeight                = 256;
		static constexpr int defaultRefreshInterval       = 100;
		static constexpr int defaultAnkiConnectionTimeout = 0;
		static constexpr int defaultAnkiConnectPort = 8765;

		using FilePath = std::filesystem::path;

		explicit Config(const FilePath& path);

		// GETTERS //
		FilePath getRootPath() const;

		FilePath getOcrRootPath() const;

		std::optional<FilePath> getKeysPath() const;

		std::optional<FilePath> getDetModelPath() const;

		std::optional<FilePath> getDetParamPath() const;

		std::optional<FilePath> getRecModelPath() const;

		std::optional<FilePath> getRecParamPath() const;

		std::optional<FilePath> getDictPath() const;

		bool getRefresh() const;

		int getRefreshIntervalMs() const;

		std::optional<FilePath> getStyle() const;

		int getTooltipWidth() const;

		int getTooltipHeight() const;

		bool hasAnki() const;

		std::optional<std::string> getAnkiNoteType() const;

		std::optional<std::string> getAnkiDeckName() const;

		std::optional<std::vector<std::string>> getAnkiTags();

		// ReSharper disable once CppInconsistentNaming
		std::optional<std::string> getAnkiAPIKey() const;

		int getAnkiConnectionTimeoutMs() const;

		int getAnkiPort();

		std::optional<std::map<std::string, std::string>> getAnkiNoteFieldValues() const;

		// SETTERS //
		void fillDefault();

		void fillEmptyDefault();

		void fillAnki(const std::unordered_set<std::string>& field_names = {});

		void setAnkiDeckName(const std::string& deck_name);

		void setAnkiNoteType(const std::string& note_type);

		// ReSharper disable once CppInconsistentNaming
		void setAnkiAPIKey(const std::string& api_key);

		void setAnkiConnectionTimeoutMs(const std::string& timeout);

		void fillAnkiNoteFields(const std::unordered_map<std::string, std::string>& field_values);

	private:
		enum class ModelType {
			Rec,
			Det
		};

		enum class FileType {
			Model,
			Param
		};

		YAML::Node node;

		FilePath config_path;
		FilePath file_root;
		FilePath ocr_file_root;

		// ReSharper disable once CppInconsistentNaming
		std::optional<FilePath> getOCRFile(ModelType model_type, FileType file_type) const;

		template<spdlog::level::level_enum NotDefinedLevel, spdlog::level::level_enum IsNullLevel>
		static bool has(
			const YAML::Node&                           node,
			const std::string&                          item,
			const std::string&                          item_prefix     = "",
			const spdlog::format_string_t<std::string>& fmt_not_defined = "{} not found",
			const spdlog::format_string_t<std::string>& fmt_is_null     = "{} is null") {
			// function definition start
			if (!node[item].IsDefined()) {
				spdlog::log(NotDefinedLevel, fmt_not_defined, item_prefix + item);
				return false;
			}

			if (node[item].IsNull()) {
				spdlog::log(IsNullLevel, fmt_is_null, item_prefix + item);
				return false;
			}

			return true;
		}

		template<typename T, spdlog::level::level_enum NotDefinedLevel, spdlog::level::level_enum IsNullLevel>
		static std::optional<T> get(
			const YAML::Node&                           node,
			const std::string&                          item,
			const std::string&                          item_prefix     = "",
			const spdlog::format_string_t<std::string>& fmt_not_defined = "{} not found",
			const spdlog::format_string_t<std::string>& fmt_is_null     = "{} is null") {
			// function definition start
			if (!has<NotDefinedLevel, IsNullLevel>(node, item, item_prefix, fmt_not_defined, fmt_is_null)) {
				return std::nullopt;
			}

			return node[item].as<T>();
		}

		template<bool IsDirectory, spdlog::level::level_enum NotDefinedLevel, spdlog::level::level_enum IsNullLevel,
		         spdlog::level::level_enum PathNotValidLevel, spdlog::level::level_enum PathPointsToWrongTypeLevel>
		static std::optional<FilePath> getFile(
			const YAML::Node&                                        node,
			const std::string&                                       item,
			const std::string&                                       item_prefix        = "",
			const FilePath&                                          parent_path        = FilePath(),
			const spdlog::format_string_t<std::string>&              fmt_not_defined    = "{} not found",
			const spdlog::format_string_t<std::string>&              fmt_is_null        = "{} is null",
			const spdlog::format_string_t<std::string, std::string>& fmt_not_valid_path =
					"The value of {} ({}) does not point to anything",
			const spdlog::format_string_t<std::string, std::string, std::string>& fmt_wrong_type =
					"The value of {} ({}) does not point to a {}") {
			// function definition start
			std::optional path_opt = get<std::string, NotDefinedLevel, IsNullLevel>(
				node,
				item,
				item_prefix,
				fmt_not_defined,
				fmt_is_null
			);

			if (!path_opt.has_value()) {
				return std::nullopt;
			}

			FilePath path = path_opt.value();

			if (path.is_relative()) {
				path = parent_path / path;
			}

			if (!exists(path)) {
				spdlog::log(PathNotValidLevel, fmt_not_valid_path, item_prefix + item, path.string());
				return std::nullopt;
			}

			if constexpr (IsDirectory) {
				if (!is_directory(path)) {
					spdlog::log(
						PathPointsToWrongTypeLevel,
						fmt_wrong_type,
						item_prefix + item,
						path.string(),
						std::string("directory")
					);
					return std::nullopt;
				}
			} else {
				if (!is_regular_file(path)) {
					spdlog::log(
						PathPointsToWrongTypeLevel,
						fmt_wrong_type,
						item_prefix + item,
						path.string(),
						std::string("file")
					);
					return std::nullopt;
				}
			}

			spdlog::info("{} found at {}", item, path.string());
			return path;
		}

		static std::optional<int> getTime(const std::optional<std::string>& time_as_string_opt);

		static std::string enum2String(ModelType model_type);

		static std::string enum2String(FileType file_type);
	};
} // ocr
