//
// Created by rowan on 14/11/2025.
//

#pragma once
#include <array>
#include <filesystem>

#include <yaml-cpp/yaml.h>

namespace ocr {
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

		bool getRefresh();
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

		static inline const std::vector<std::string> suffixes = {
			"",
			"file",
			"filepath",
			"file-path",
			"file_path",
			"path"
		};

		static inline const std::vector<std::string> root_alias = {
			"file_root",
			"file_root_path",
			"file-root",
			"file-root-path",
			"root",
			"root_path",
			"root-path",
			"model_root",
			"model_root_path",
			"model-root",
			"model-root-path"
		};

		static inline const std::vector<std::string> model_name_alias = {"model_name", "model-name", "name"};


		static inline const std::vector<std::string> model_alias = {"model", "bin"};
		static inline const std::vector<std::string> model_ext   = {"bin"};
		static inline const std::vector<std::string> param_alias = {"param", "params"};
		static inline const std::vector<std::string> param_ext   = {"param"};

		static inline const std::vector<std::string> det_alias = {"det"};
		static inline const std::vector<std::string> rec_alias = {"rec"};

		static inline const std::vector<std::string> key_alias = {
			"key",
			"keys",
			"letter",
			"letters",
			"character",
			"characters",
			"char",
			"chars"
		};

		static inline const std::vector<std::string> connectors = {"", "-", "_"};

		file_path config_path;
		file_path file_root;

		file_path getPath(ModelType model_type, FileType file_type);

		static std::string enum2String(ModelType model_type);

		static std::string enum2String(FileType file_type);
	};
} // ocr
