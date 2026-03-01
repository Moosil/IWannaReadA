#include "config.h"

#include <format>
#include <spdlog/spdlog.h>

#include "util.h"

namespace ocr {
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
		for (const std::string& alias : root_alias) {
			if (node[alias]) {
				file_path path = node[alias].as<std::string>();
				if (path.empty()) {
					continue;
				}
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
		for (const std::string& alias : key_alias) {
			for (const std::string& connector : connectors) {
				for (const std::string& suffix : suffixes) {
					std::string curr_name = alias;
					curr_name.append(connector).append(suffix);
					if (node[curr_name]) {
						if (file_path path = file_root / node[curr_name].as<std::string>();
							std::filesystem::is_regular_file(path)) {
							spdlog::info("found key path at {}", path.string());
							return path;
						}
					}
				}
			}
		}
		throw std::runtime_error{std::format("couldn't find key in {}", config_path.string())};
	}


	Config::file_path Config::getPath(const ModelType model_type, const FileType file_type) {
		const FileType opposite_file_type = file_type == FileType::Model ? FileType::Param : FileType::Model;
		const std::vector<std::string>& model_type_alias = model_type == ModelType::Det ? det_alias : rec_alias;
		const std::vector<std::string>& file_type_alias = file_type == FileType::Model ? model_alias : param_alias;
		const std::vector<std::string>& opposite_file_type_alias = file_type == FileType::Model
		                                                           ? param_alias
		                                                           : model_alias;
		const std::vector<std::string>& file_extensions         = file_type == FileType::Model ? model_ext : param_ext;
		const std::string               model_type_name         = enum2String(model_type);
		const std::string               file_type_name          = enum2String(file_type);
		const std::string               opposite_file_type_name = enum2String(opposite_file_type);

		spdlog::info("looking for {} {} path...", model_type_name, file_type_name);
		spdlog::info("looking for model name...");
		for (const std::string& alias : model_name_alias) {
			if (node[alias]) {
				file_path name = node[alias].as<std::string>();
				if (name.empty()) {
					continue;
				}
				spdlog::info("found model name: {}", name.string());
				for (const std::string& connector : connectors) {
					for (const std::string& det : model_type_alias) {
						for (const std::string& ext : file_extensions) {
							if (file_path path = (file_root / name / connector / det).replace_extension(ext);
								std::filesystem::is_regular_file(path)) {
								spdlog::info("found {} {} path at {}", model_type_name, file_type_name, path.string());
								return path;
							}
						}
					}
				}
			}
		}
		spdlog::info("looking for {} structure...", model_type_name);
		for (const std::string& det : model_type_alias) {
			if (node[det]) {
				spdlog::info("found {0} structure. looking for {0} {1} path...", model_type_name, file_type_name);
				YAML::Node det_node = node[det];
				for (const std::string& alias : file_type_alias) {
					for (const std::string& connector : connectors) {
						for (const std::string& suffix : suffixes) {
							std::string curr_name = alias;
							curr_name.append(connector).append(suffix);
							if (det_node[curr_name]) {
								if (file_path path = file_root / det_node[curr_name].as<std::string>();
									std::filesystem::is_regular_file(path)) {
									spdlog::info(
										"found {} {} path at {}",
										model_type_name,
										file_type_name,
										path.string()
									);
									return path;
								}
							}
						}
					}
				}

				spdlog::info("looking for {} {} path...", model_type_name, opposite_file_type_name);
				for (const std::string& alias : opposite_file_type_alias) {
					for (const std::string& connector : connectors) {
						for (const std::string& suffix : suffixes) {
							std::string curr_name = alias;
							curr_name.append(connector).append(suffix);
							if (det_node[curr_name]) {
								if (file_path param_path = file_root / det_node[curr_name].as<std::string>();
									std::filesystem::is_regular_file(param_path)) {
									for (const std::string& ext : file_extensions) {
										if (file_path model_path = param_path.replace_extension(ext);
											std::filesystem::is_regular_file(model_path)) {
											spdlog::info(
												"found {} {} path at {}",
												model_type_name,
												file_type_name,
												model_path.string()
											);
											return model_path;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		spdlog::info("looking for {} {} path outside of structure...", model_type_name, file_type_name);
		for (const std::string& det : model_type_alias) {
			for (const std::string& connector0 : connectors) {
				for (const std::string& alias : file_type_alias) {
					for (const std::string& connector1 : connectors) {
						for (const std::string& suffix : suffixes) {
							std::string curr_name = det;
							curr_name.append(connector0).append(alias).append(connector1).append(suffix);
							if (node[curr_name]) {
								if (file_path path = file_root / node[curr_name].as<std::string>();
									std::filesystem::is_regular_file(path)) {
									spdlog::info(
										"found {} {} path at {}",
										model_type_name,
										file_type_name,
										path.string()
									);
									return path;
								}
							}
						}
					}
				}
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

	bool Config::getRefresh() {
		for (const auto& refresh : refresh_alias)
			if (node[refresh])
				return node[refresh].as<bool>();
		return false;
	}

	int Config::getRefreshIntervalMs() {
		std::string string_duration = getRefreshIntervalAsString();
		trim(string_duration);
		int value{};
		if (auto [ptr, ec] = std::from_chars(string_duration.data(), string_duration.data() + string_duration.size(), value); ec == std::errc{}) {
			const std::string extra{ptr};
			if (extra.empty())
				return value * 1000;
			if (extra == "ms")
				return value;
		}
		return -1;
	}

	std::string Config::getRefreshIntervalAsString() {
		for (const auto& interval : refresh_interval_alias)
			return node[interval].as<std::string>();
		return "-1";
	}
} // ocr
