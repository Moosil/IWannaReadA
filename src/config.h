#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>

namespace iwra {
	class Config {
	public:
		using FilePath = std::filesystem::path;

		explicit Config(const FilePath& path);

		FilePath getRootPath() const;

		FilePath getOcrRootPath() const;

		FilePath getKeyPath() const;

		FilePath getDetModelPath() const;

		FilePath getDetParamPath() const;

		FilePath getRecModelPath() const;

		FilePath getRecParamPath() const;

		FilePath getDictPath() const;

		std::optional<std::string> getAnkiCardType() const;

		std::optional<std::string> getAnkiDeckName() const;

		bool getRefresh() const;

		std::optional<int> getRefreshIntervalMs() const;

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

		FilePath getPath(ModelType model_type, FileType file_type) const;

		std::optional<std::string> getRefreshIntervalAsString() const;

		static std::string enum2String(ModelType model_type);

		static std::string enum2String(FileType file_type);
	};
} // ocr
