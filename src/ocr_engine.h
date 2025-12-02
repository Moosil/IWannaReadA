#pragma once

#include <filesystem>

#include "config.h"
#include "det.h"
#include "rec.h"

namespace ocr {
	class OCREngine {
	private:
		Det det;
		Rec rec;
	public:
		OCREngine(
			const std::string& det_model_path,
			const std::string& det_param_path,
			const std::string& rec_model_path,
			const std::string& rec_param_path,
			const std::string& keys_path
		);

		OCREngine(
			const std::filesystem::path& det_model_path,
			const std::filesystem::path& det_param_path,
			const std::filesystem::path& rec_model_path,
			const std::filesystem::path& rec_param_path,
			const std::filesystem::path& keys_path
		);

		OCREngine(
			const char* det_model_path,
			const char* det_param_path,
			const char* rec_model_path,
			const char* rec_param_path,
			const char* keys_path
		);

		explicit OCREngine(Config& yaml);

		std::vector<OCRResult> run(const std::string& image_path) const;

		std::vector<OCRResult> run(const cv::Mat& image) const;
	};
}
