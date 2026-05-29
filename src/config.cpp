#include "config.h"

#include <format>

#include "util_text.h"

namespace iwra {
	Config::Config(const std::filesystem::path& path):
		node{YAML::LoadFile(path.string())},
		config_path{path} {
		file_root     = getRootPath();
		ocr_file_root = getOcrRootPath();
	}

	Config::FilePath Config::getRootPath() const {
		return getFile<true, spdlog::level::info, spdlog::level::info, spdlog::level::warn, spdlog::level::err>(
			node,
			"file-root",
			"",
			file_root,
			"{} not found. Defaulting to config file parent path. Set a file root to set where the files are kept",
			"{} is null. Defaulting to config file parent path. Set a file root to set where the files are kept",
			"The value of {} ({}) is not a valid path. Defaulting to config file parent path. Set a file root to set where the files are kept",
			"The value of {} ({}) does not point to a {}. Defaulting to config file parent path. Set a file root to set where the files are kept"
		).value_or(config_path.parent_path());
	}

	Config::FilePath Config::getOcrRootPath() const {
		return getFile<true, spdlog::level::info, spdlog::level::info, spdlog::level::warn, spdlog::level::err>(
			node["ocr"],
			"file-root",
			"ocr ",
			file_root,
			"{} not found. Defaulting to config file parent path. Set a ocr file root to set where the ocr files are kept",
			"{} is null. Defaulting to config file parent path. Set a ocr file root to set where the ocr files are kept",
			"The value of {} ({}) is not a valid path. Defaulting to config file parent path. Set a ocr file root to set where the ocr files are kept",
			"The value of {} ({}) does not point to a {}. Defaulting to config file parent path. Set a ocr file root to set where the ocr files are kept"
		).value_or(file_root);
	}

	std::optional<Config::FilePath> Config::getKeysPath() const {
		return getFile<false, spdlog::level::err, spdlog::level::err, spdlog::level::err, spdlog::level::err>(
			node["ocr"],
			"keys-path",
			"ocr ",
			ocr_file_root
		);
	}

	std::optional<Config::FilePath> Config::getDetModelPath() const {
		return getPath(ModelType::Det, FileType::Model);
	}

	std::optional<Config::FilePath> Config::getDetParamPath() const {
		return getPath(ModelType::Det, FileType::Param);
	}

	std::optional<Config::FilePath> Config::getRecModelPath() const {
		return getPath(ModelType::Rec, FileType::Model);
	}

	std::optional<Config::FilePath> Config::getRecParamPath() const {
		return getPath(ModelType::Rec, FileType::Param);
	}

	Config::FilePath Config::getDictPath() const {
		return getFile<false, spdlog::level::err, spdlog::level::err, spdlog::level::err, spdlog::level::err>(
			node,
			"dictionary-path",
			"",
			file_root
		).value();
	}

	bool Config::getRefresh() const {
		return get<bool, spdlog::level::warn, spdlog::level::info>(node, "refresh").value_or(false);
	}

	std::optional<int> Config::getRefreshIntervalMs() const {
		const auto opt = getRefreshIntervalAsString();
		if (!opt.has_value()) {
			return std::nullopt;
		}

		std::string string_duration = opt.value();
		trim(string_duration);
		int value{};

		auto [ptr, ec] = std::from_chars(
			string_duration.data(),
			string_duration.data() + string_duration.size(),
			value
		);
		if (ec != std::errc{}) {
			return std::nullopt;
		}

		const std::string extra{ptr};
		if (extra.empty()) {
			return value * 1000;
		}

		if (extra == "ms") {
			return value;
		}

		spdlog::warn("unknown unit for refresh provided: {}, defaulting to ms", extra);
		return value;
	}

	std::optional<Config::FilePath> Config::getStyle() const {
		return getFile<false, spdlog::level::warn, spdlog::level::info, spdlog::level::warn, spdlog::level::warn>(
			node,
			"style",
			"",
			"{} not found. Implicitly using default style. Set to null (~) to explicitly disable or set a value to use a custom qss file",
			"{} is null. Explicitly using default style. Set a value to use a custom qss file"
		);
	}

	int Config::getTooltipWidth() const {
		return get<int, spdlog::level::warn, spdlog::level::warn>(
			node,
			"width",
			"",
			"{} not found. Implicitly using default tooltip window width. Set to null (~) to explicitly disable or set a value to customise it",
			"{} is null. Using default tooltip window width. Set a value to customise it"
		).value_or(defaultWidth);
	}

	int Config::getTooltipHeight() const {
		return get<int, spdlog::level::warn, spdlog::level::warn>(
			node,
			"height",
			"",
			"{} not found. Using default tooltip window height. Set to null (~) to explicitly disable or set a value to customise it",
			"{} is null. Using default tooltip window height. Set a value to customise it"
		).value_or(defaultWidth);
	}

	bool Config::hasAnki() const {
		return has<spdlog::level::warn, spdlog::level::info>(
			node,
			"anki",
			"",
			"{} not found. Implicitly disabling Anki. Set to null (~) to explicitly disable or set a value to use Anki",
			"{} is null. Explicitly disabling Anki. Set a value to use Anki"
		);
	}

	std::optional<std::string> Config::getAnkiCardType() const {
		if (!hasAnki()) {
			return std::nullopt;
		}

		return get<std::string, spdlog::level::err, spdlog::level::err>(
			node["anki"],
			"card-type",
			"anki ",
			"{} not found. Implicitly disabling Anki. Set a value to allow Anki integration",
			"{} is null. Implicitly disabling Anki. Set a value to allow Anki integration"
		);
	}

	std::optional<std::string> Config::getAnkiDeckName() const {
		if (!hasAnki()) {
			return std::nullopt;
		}

		return get<std::string, spdlog::level::err, spdlog::level::err>(
			node["anki"],
			"deck-name",
			"anki "
			"{} not found. Implicitly disabling Anki. Set a value to allow Anki integration",
			"{} is null. Implicitly disabling Anki. Set a value to allow Anki integration"
		);
	}

	// ReSharper disable once CppInconsistentNaming
	std::optional<std::string> Config::getAnkiAPIKey() const {
		if (!hasAnki()) {
			return std::nullopt;
		}

		return get<std::string, spdlog::level::err, spdlog::level::err>(
			node["anki"],
			"api-key",
			"anki ",
			"{} not found. Implicitly disabling Anki. Set a value or disable AnkiConnect API key to allow Anki integration",
			"{} is null. Implicitly disabling Anki. Set a value or disable AnkiConnect API key to allow Anki integration"
		);
	}

	std::optional<Config::FilePath> Config::getPath(const ModelType model_type, const FileType file_type) const {
		const std::string model_type_name = enum2String(model_type);
		const std::string file_type_name  = enum2String(file_type);

		const bool has_model = has<spdlog::level::err, spdlog::level::err>(
			node["ocr"],
			model_type_name,
			"ocr",
			"{} not found. Set a ocr file paths to use the program",
			"{} is null. Set a ocr file paths to use the program"
		);

		if (!has_model) {
			return std::nullopt;
		}

		return getFile<false, spdlog::level::err, spdlog::level::err, spdlog::level::err, spdlog::level::err>(
			node["ocr"][model_type_name],
			file_type_name + "-path",
			"ocr " + model_type_name + " ",
			ocr_file_root,
			"{} not found. Set a ocr file paths to use the program",
			"{} is null. Set a ocr file paths to use the program",
			"The value of {} ({}) is not a valid path. Set a valid paths to use the program",
			"The value of {} ({}) does not point to a {}. Set a valid paths to use the program"
		).value();
	}

	std::optional<std::string> Config::getRefreshIntervalAsString() const {
		return get<std::string, spdlog::level::warn, spdlog::level::info>(
			node["anki"],
			"refresh-interval",
			""
			"{} not found. Implicitly using default refresh interval. Set to null (~) to explicitly use the default or set a value to customise it",
			"{} is null. Explicitly using default refresh interval. Set a value to customise it"
		);
	}

	// ReSharper disable once CppNotAllPathsReturnValue
	std::string Config::enum2String(const ModelType model_type) {
		switch (model_type) {
			case ModelType::Det: return "det";
			case ModelType::Rec: return "rec";
		}
	}

	// ReSharper disable once CppNotAllPathsReturnValue
	std::string Config::enum2String(const FileType file_type) {
		switch (file_type) {
			case FileType::Model: return "model";
			case FileType::Param: return "param";
		}
	}
} // ocr
