#include "config.h"

#include <variant>

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
			"The value of {} ({}) does not point to anything. Defaulting to config file parent path. Set a file root to set where the files are kept",
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
			"The value of {} ({}) does not point to anything. Defaulting to config file parent path. Set a ocr file root to set where the ocr files are kept",
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
		return getOCRFile(ModelType::Det, FileType::Model);
	}

	std::optional<Config::FilePath> Config::getDetParamPath() const {
		return getOCRFile(ModelType::Det, FileType::Param);
	}

	std::optional<Config::FilePath> Config::getRecModelPath() const {
		return getOCRFile(ModelType::Rec, FileType::Model);
	}

	std::optional<Config::FilePath> Config::getRecParamPath() const {
		return getOCRFile(ModelType::Rec, FileType::Param);
	}

	std::optional<Config::FilePath> Config::getDictPath() const {
		return getFile<false, spdlog::level::err, spdlog::level::err, spdlog::level::err, spdlog::level::err>(
			node,
			"dictionary-path",
			"",
			file_root
		);
	}

	bool Config::getRefresh() const {
		return get<bool, spdlog::level::warn, spdlog::level::info>(
			node,
			"refresh",
			"{} not found. Implicitly disabling tooltip refresh. Set to null (~) to explicitly disable or set a value to customise it",
			"{} is null. Disabling tooltip refresh. Set a value to customise it"
		).value_or(false);
	}

	int Config::getRefreshIntervalMs() const {
		return getTime(
			get<std::string, spdlog::level::warn, spdlog::level::info>(
				node["anki"],
				"refresh-interval",
				"",
				"{} not found. Implicitly using default refresh interval. Set to null (~) to explicitly use the default or set a value to customise it",
				"{} is null. Using default refresh interval. Set a value to customise it"
			)
		).value_or(defaultRefreshInterval);
	}

	std::optional<Config::FilePath> Config::getStyle() const {
		return getFile<false, spdlog::level::warn, spdlog::level::info, spdlog::level::warn, spdlog::level::warn>(
			node,
			"style-path",
			"",
			file_root,
			"{} not found. Implicitly using default style. Set to null (~) to explicitly disable or set a value to use a custom qss file",
			"{} is null. Using default style. Set a value to use a custom qss file",
			"The value of {} ({}) does not point to anything. Set a value to use a custom qss file",
			"The value of {} ({}) does not point to a {}. Set a value to use a custom qss file"
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
			"{} not found. Implicitly using default tooltip window height. Set to null (~) to explicitly disable or set a value to customise it",
			"{} is null. Using default tooltip window height. Set a value to customise it"
		).value_or(defaultWidth);
	}

	bool Config::hasAnki() const {
		return has<spdlog::level::warn, spdlog::level::info>(
			node,
			"anki",
			"",
			"{} not found. Implicitly disabling Anki. Set to null (~) to explicitly disable or set a value to use Anki",
			"{} is null. Disabling Anki. Set a value to use Anki"
		);
	}

	std::optional<std::string> Config::getAnkiNoteType() const {
		if (!hasAnki()) {
			return std::nullopt;
		}

		return get<std::string, spdlog::level::err, spdlog::level::err>(
			node["anki"],
			"note-type",
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

	std::optional<std::vector<std::string>> Config::getAnkiTags() {
		if (!hasAnki()) {
			return std::nullopt;
		}

		return get<std::vector<std::string>, spdlog::level::warn, spdlog::level::info>(
			node["anki"],
			"note-tags",
			"anki ",
			"{} not found. Implicitly using no tags. Set to null (~) to explicitly disable or set a value to add note tags",
			"{} is null. Implicitly disabling Anki. Set a value to add note tags"
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
			"{} not found. Implicitly disabling Anki. Set a value  to allow Anki integration",
			"{} is null. Implicitly disabling Anki. Set a value  to allow Anki integration"
		);
	}

	int Config::getAnkiConnectionTimeoutMs() const {
		if (!hasAnki()) {
			return defaultAnkiConnectionTimeout;
		}

		return getTime(
			get<std::string, spdlog::level::err, spdlog::level::err>(
				node["anki"],
				"connection-timeout",
				"anki ",
				"{} not found. Implicitly using default AnkiConnect connection timeout. Set to null (~) to explicitly disable or set a value to customise it",
				"{} is null. Using default AnkiConnect connection timeout. Set a value to customise it"
			)
		).value_or(defaultAnkiConnectionTimeout);
	}

	int Config::getAnkiPort() {
		if (!hasAnki()) {
			return defaultAnkiConnectPort;
		}

		return getTime(
			get<std::string, spdlog::level::warn, spdlog::level::info>(
				node["anki"],
				"port",
				"anki ",
				"{} not found. Implicitly using default AnkiConnect port. Set to null (~) to explicitly disable or set a value to customise it",
				"{} is null. Using default AnkiConnect port. Set a value to customise it"
			)
		).value_or(defaultAnkiConnectPort);
	}

	std::optional<std::unordered_map<std::string, std::string> > Config::getAnkiNoteFieldValues() const {
		if (!hasAnki()) {
			return std::nullopt;
		}

		return get<std::unordered_map<std::string, std::string>, spdlog::level::err, spdlog::level::err>(
			node["anki"],
			"note-fields",
			"anki ",
			"{} not found. Implicitly disabling Anki. Set values to enable Anki integration",
			"{} is null. Implicitly disabling Anki. Set values to enable Anki integration"
		);
	}

	void Config::fillDefault() {
		node["file-root"] = ".";

		node["ocr"]["file-root"]         = YAML::Null;
		node["ocr"]["keys-path"]         = YAML::Null;
		node["ocr"]["det"]["model-path"] = YAML::Null;
		node["ocr"]["det"]["param-path"] = YAML::Null;
		node["ocr"]["rec"]["model-path"] = YAML::Null;
		node["ocr"]["rec"]["param-path"] = YAML::Null;

		node["refresh"]          = false;
		node["refresh-interval"] = defaultRefreshInterval;

		node["dictionary-path"] = YAML::Null;

		node["style-path"] = YAML::Null;
		node["width"]      = defaultWidth;
		node["height"]     = defaultHeight;

		node["anki"] = YAML::Null;
	}

	void Config::fillEmptyDefault() {
		if (!node["file-root"].IsDefined()) {
			node["file-root"] = ".";
		}

		if (!node["ocr"]["file-root"].IsDefined()) {
			node["ocr"]["file-root"] = YAML::Null;
		}
		if (!node["ocr"]["keys-path"].IsDefined()) {
			node["ocr"]["keys-path"] = YAML::Null;
		}
		if (!node["ocr"]["det"]["model-path"].IsDefined()) {
			node["ocr"]["det"]["model-path"] = YAML::Null;
		}
		if (!node["ocr"]["det"]["param-path"].IsDefined()) {
			node["ocr"]["det"]["param-path"] = YAML::Null;
		}
		if (!node["ocr"]["rec"]["model-path"].IsDefined()) {
			node["ocr"]["rec"]["model-path"] = YAML::Null;
		}
		if (!node["ocr"]["rec"]["param-path"].IsDefined()) {
			node["ocr"]["rec"]["param-path"] = YAML::Null;
		}

		if (!node["refresh"].IsDefined()) {
			node["refresh"] = false;
		}
		if (!node["refresh-interval"].IsDefined()) {
			node["refresh-interval"] = defaultRefreshInterval;
		}

		if (!node["dictionary-path"].IsDefined()) {
			node["dictionary-path"] = YAML::Null;
		}

		if (!node["style-path"].IsDefined()) {
			node["style-path"] = YAML::Null;
		}
		if (!node["width"].IsDefined()) {
			node["width"] = defaultWidth;
		}
		if (!node["height"].IsDefined()) {
			node["height"] = defaultHeight;
		}

		if (!node["anki"].IsDefined()) {
			node["anki"] = YAML::Null;
		}
	}

	void Config::fillAnki(const std::unordered_set<std::string>& field_names) {
		if (!hasAnki()) {
			node["anki"] = {};
		}

		if (!getAnkiNoteType().has_value()) {
			node["anki"]["note-type"] = YAML::Null;
		}

		if (!getAnkiDeckName().has_value()) {
			node["anki"]["deck-name"] = YAML::Null;
		}

		if (!getAnkiAPIKey().has_value()) {
			node["anki"]["api-key"] = YAML::Null;
		}

		if (getAnkiConnectionTimeoutMs() == defaultAnkiConnectionTimeout) {
			node["anki"]["connection-timeout"] = defaultAnkiConnectionTimeout;
		}

		for (const auto& key : field_names) {
			node["anki"]["note-fields"][key] = "";
		}
	}

	void Config::setAnkiDeckName(const std::string& deck_name) {
		fillAnki();
		node["anki"]["deck-name"] = deck_name;
	}

	void Config::setAnkiNoteType(const std::string& note_type) {
		fillAnki();
		node["anki"]["note-type"] = note_type;
	}

	// ReSharper disable once CppInconsistentNaming
	void Config::setAnkiAPIKey(const std::string& api_key) {
		fillAnki();
		node["anki"]["api-key"] = api_key;
	}

	void Config::setAnkiConnectionTimeoutMs(const std::string& timeout) {
		fillAnki();
		node["anki"]["connection-timeout"] = timeout;
	}

	void Config::fillAnkiNoteFields(const std::unordered_map<std::string, std::string>& field_values) {
		fillAnki();
		node["anki"]["note-fields"] = field_values;
	}

	// ReSharper disable once CppInconsistentNaming
	std::optional<Config::FilePath> Config::getOCRFile(const ModelType model_type, const FileType file_type) const {
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

	std::optional<int> Config::getTime(const std::optional<std::string>& time_as_string_opt) {
		if (!time_as_string_opt.has_value()) {
			return std::nullopt;
		}

		std::string string_duration = time_as_string_opt.value();
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
