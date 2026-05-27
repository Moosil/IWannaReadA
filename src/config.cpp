#include "config.h"

#include <format>
#include <spdlog/spdlog.h>

#include "util_text.h"

namespace iwra {
	Config::Config(const std::filesystem::path& path):
		node{YAML::LoadFile(path.string())},
		config_path{path} {
		file_root     = getRootPath();
		ocr_file_root = getOcrRootPath();
	}

	Config::FilePath Config::getRootPath() const {
		spdlog::info("looking for root path...");
		if (node["file-root"]) {
			if (FilePath path = node["file-root"].as<std::string>();
				!path.empty()) {
				if (path.is_relative()) {
					if (FilePath res = config_path.parent_path() / path;
						is_directory(res)) {
						spdlog::info("found root path at {}", res.string());
						return res;
					}
				}
				if (is_directory(path)) {
					spdlog::info("found root path at {}", path.string());
					return path;
				}
			}
		}
		const FilePath path = config_path.parent_path();
		spdlog::info("not found root path. Defaulting to {}", path.string());
		return path;
	}

	Config::FilePath Config::getOcrRootPath() const {
		spdlog::info("looking for ocr root path...");
		if (node["ocr"]["file-root"]) {
			if (FilePath path = node["ocr"]["file-root"].as<std::string>();
				!path.empty()) {
				if (path.is_relative()) {
					if (FilePath res = config_path.parent_path() / path;
						is_directory(res)) {
						spdlog::info("found ocr root path at {}", res.string());
						return res;
					}
				}
				if (is_directory(path)) {
					spdlog::info("found ocr root path at {}", path.string());
					return path;
				}
			}
		}
		const FilePath path = config_path.parent_path();
		spdlog::info("not found ocr root path. Defaulting to {}", path.string());
		return path;
	}

	Config::FilePath Config::getKeyPath() const {
		spdlog::info("looking for key path...");
		if (node["ocr"]["keys-path"]) {
			if (FilePath path = ocr_file_root / node["ocr"]["keys-path"].as<std::string>();
				is_regular_file(path)) {
				spdlog::info("found key path at {}", path.string());
				return path;
			}
		}
		throw std::runtime_error{std::format("couldn't find key in {}", config_path.string())};
	}

	Config::FilePath Config::getDetModelPath() const {
		return getPath(ModelType::Det, FileType::Model);
	}

	Config::FilePath Config::getDetParamPath() const {
		return getPath(ModelType::Det, FileType::Param);
	}

	Config::FilePath Config::getRecModelPath() const {
		return getPath(ModelType::Rec, FileType::Model);
	}

	Config::FilePath Config::getRecParamPath() const {
		return getPath(ModelType::Rec, FileType::Param);
	}

	Config::FilePath Config::getDictPath() const {
		if (node["dictionary-path"]) {
			if (FilePath path = config_path.parent_path() / node["dictionary-path"].as<std::string>();
				exists(path)) {
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

	std::optional<std::string> Config::getAnkiCardType() const {
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

	std::optional<std::string> Config::getAnkiDeckName() const {
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

	bool Config::getRefresh() const {
		if (node["refresh"]) {
			return node["refresh"].as<bool>();
		}
		spdlog::warn("couldn't find refresh in {}", config_path.string());
		return false;
	}

	std::optional<int> Config::getRefreshIntervalMs() const {
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
			);
			ec == std::errc{}) {
			const std::string extra{ptr};
			if (extra.empty()) {
				return value * 1000;
			}
			if (extra == "ms") {
				return value;
			}
		}
		return std::nullopt;
	}

	Config::FilePath Config::getPath(const ModelType model_type, const FileType file_type) const {
		const std::string model_type_name = enum2String(model_type);
		const std::string file_type_name  = enum2String(file_type);

		if (node["ocr"][model_type_name][file_type_name + "-path"]) {
			if (FilePath path = ocr_file_root / node["ocr"][model_type_name][file_type_name + "-path"].as<
				                     std::string>();
				is_regular_file(path)) {
				spdlog::info("found key path at {}", path.string());
				return path;
			}
		}

		spdlog::error("couldn't find {} {} in {}", model_type_name, file_type_name, config_path.string());
		throw std::runtime_error{
			std::format("couldn't find {} {} in {}", model_type_name, file_type_name, config_path.string())
		};
	}

	std::optional<std::string> Config::getRefreshIntervalAsString() const {
		if (node["refresh-interval"]) {
			return node["refresh-interval"].as<std::string>();
		}
		spdlog::warn("couldn't find refresh interval in {}", config_path.string());
		return std::nullopt;
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
