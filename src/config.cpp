#include "config.h"


#include <format>
#include <spdlog/spdlog.h>

#include "util_text.h"


namespace iwra {
	Config::Config(const std::filesystem::path& path) :
		node{YAML::LoadFile(path.string())},
		config_path{path} {
		file_root = getRootPath();
	}

	// ReSharper disable once CppNotAllPathsReturnValue
	std::string Config::enum2String(const ModelType model_type) {
		switch (model_type) {
			case ModelType::Det:
				return "det";
			case ModelType::Rec:
				return "rec";
		}
	}

	// ReSharper disable once CppNotAllPathsReturnValue
	std::string Config::enum2String(const FileType file_type) {
		switch (file_type) {
			case FileType::Model:
				return "model";
			case FileType::Param:
				return "param";
		}
	}

	Config::file_path Config::getRootPath() {
		spdlog::info("looking for root path...");
		if (node["file-root"]) {
			file_path path = node["file-root"].as<std::string>();
			if (!path.empty()) {
				if (path.is_relative()) {
					if (file_path res = config_path.parent_path() / path; std::filesystem::is_directory(res)) {
						spdlog::info("found root path at {}", res.string());
						return res;
					}
				}
				if (std::filesystem::is_directory(path)) {
					spdlog::info("found root path at {}", path.string());
					return path;
				}
			}
		}
		const file_path path = config_path.parent_path();
		spdlog::info("not found root path. Defaulting to {}", path.string());
		return path;
	}


	Config::file_path Config::getKeyPath() {
		spdlog::info("looking for key path...");
		if (node["ocr"]["keys-path"]) {
			if (file_path path = file_root/ node["ocr"]["keys-path"].as<std::string>();
				std::filesystem::is_regular_file(path)) {
				spdlog::info("found key path at {}", path.string());
				return path;
			}
		}
		throw std::runtime_error{std::format("couldn't find key in {}", config_path.string())};
	}


	Config::file_path Config::getPath(const ModelType model_type, const FileType file_type) {
		const std::string               model_type_name         = enum2String(model_type);
		const std::string               file_type_name          = enum2String(file_type);

		if (node["ocr"][model_type_name][file_type_name + "-path"]) {
			if (file_path path = file_root / node["ocr"][model_type_name][file_type_name + "-path"].as<std::string>();
				std::filesystem::is_regular_file(path)) {
				spdlog::info("found key path at {}", path.string());
				return path;
			}
		}

		spdlog::error("couldn't find {} {} in {}", model_type_name, file_type_name, config_path.string());
		throw std::runtime_error{
			std::format("couldn't find {} {} in {}", model_type_name, file_type_name, config_path.string())
		};
	}

	Config::file_path Config::getDetModelPath() {
		return getPath(ModelType::Det, FileType::Model);
	}

	Config::file_path Config::getDetParamPath() {
		return getPath(ModelType::Det, FileType::Param);
	}

	Config::file_path Config::getRecModelPath() {
		return getPath(ModelType::Rec, FileType::Model);
	}

	Config::file_path Config::getRecParamPath() {
		return getPath(ModelType::Rec, FileType::Param);
	}

	Config::file_path Config::getHTMLTemplatePath() {
		if (node["html-template-path"]) {
			if (file_path path = config_path.parent_path() / node["html-template-path"].as<std::string>();
				std::filesystem::is_regular_file(path)) {
				spdlog::info("found html template path at {}", path.string());
				return path;
			}
		}
		spdlog::error("couldn't find html template path in {}", config_path.string());
		throw std::runtime_error{
			std::format("couldn't find html template path in {}", config_path.string())
		};
	}

	Config::file_path Config::getDictPath() {
		if (node["dictionary-path"]) {
			if (file_path path = config_path.parent_path() / node["dictionary-path"].as<std::string>();
				std::filesystem::exists(path)) {
				spdlog::info("found dict path at {}", path.string());
				return path;
			} else {
				spdlog::error("couldn't find dict at {}", path.string());
			}
		}
		spdlog::error("couldn't find dict in {}", config_path.string());
		throw std::runtime_error{
			std::format("couldn't find dict path in {}", config_path.string())
		};
	}

	bool Config::getRefresh() {
		if (node["refresh"]) {
			return node["refresh"].as<bool>();
		}
		spdlog::warn("couldn't find refresh in {}", config_path.string());
		return false;
	}

	std::optional<int> Config::getRefreshIntervalMs() {
		const auto opt = getRefreshIntervalAsString();
		if (!opt.has_value()) {
			return std::nullopt;
		}
		std::string string_duration = opt.value();
		trim(string_duration);
		int value{};
		if (auto [ptr, ec] = std::from_chars(
			string_duration.data(),
			string_duration.data() + string_duration.size(),
			value
		); ec == std::errc{}) {
			const std::string extra{ptr};
			if (extra.empty())
				return value * 1000;
			if (extra == "ms")
				return value;
		}
		return std::nullopt;
	}

	std::optional<std::string> Config::getRefreshIntervalAsString() {
		if (node["refresh-interval"]) {
			return node["refresh-interval"].as<std::string>();
		}
		spdlog::warn("couldn't find refresh interval in {}", config_path.string());
		return std::nullopt;
	}

	std::optional<std::string> Config::getAnkiCardType() {
		if (node["anki"]) {
			if (node["anki"]["card-type"]) {
				return node["anki"]["card-type"].as<std::string>();
			}
			spdlog::error("couldn't find anki card type in {}", config_path.string());
			throw std::runtime_error{
				std::format("couldn't find anki card type in {}", config_path.string())
			};
		}
		spdlog::warn("couldn't find anki in {}", config_path.string());
		return std::nullopt;
	}

	std::optional<std::string> Config::getAnkiDeckName() {
		if (node["anki"]) {
			if (node["anki"]["deck-name"]) {
				return node["anki"]["deck-name"].as<std::string>();
			}
			spdlog::error("couldn't find anki deck name in {}", config_path.string());
			throw std::runtime_error{
				std::format("couldn't find anki deck name in {}", config_path.string())
			};
		}
		spdlog::warn("couldn't find anki in {}", config_path.string());
		return std::nullopt;
	}
} // ocr
