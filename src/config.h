#pragma once


#include <yaml-cpp/yaml.h>

#include <filesystem>


namespace iwra {
	class Config {
	public:
		using file_path = std::filesystem::path;

		explicit Config(const file_path& path);

		file_path getRootPath();

		file_path getKeyPath();

		file_path getDetModelPath();

		file_path getDetParamPath();

		file_path getRecModelPath();

		file_path getRecParamPath();

		file_path getHTMLTemplatePath();

		file_path getDictPath();

		std::optional<std::string> getAnkiCardType();

		std::optional<std::string> getAnkiDeckName();

		bool getRefresh();

		std::optional<int> getRefreshIntervalMs();

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

		file_path config_path;
		file_path file_root;

		file_path getPath(ModelType model_type, FileType file_type);

		std::optional<std::string> getRefreshIntervalAsString();

		static std::string enum2String(ModelType model_type);

		static std::string enum2String(FileType file_type);
	};
} // ocr
