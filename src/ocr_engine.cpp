#include "ocr_engine.h"

#include <filesystem>
#include <ranges>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/mat.hpp>
#include <spdlog/spdlog.h>

#include "config.h"
#include "util_ocr.h"


namespace iwra {
	OCREngine::OCREngine(
		const std::string& det_model_path,
		const std::string& det_param_path,
		const std::string& rec_model_path,
		const std::string& rec_param_path,
		const std::string& keys_path
	):
		det{det_model_path, det_param_path},
		rec{rec_model_path, rec_param_path, keys_path} {}

	OCREngine::OCREngine(
		const std::filesystem::path& det_model_path,
		const std::filesystem::path& det_param_path,
		const std::filesystem::path& rec_model_path,
		const std::filesystem::path& rec_param_path,
		const std::filesystem::path& keys_path
	):
		det{det_model_path.string(), det_param_path.string()},
		rec{rec_model_path.string(), rec_param_path.string(), keys_path.string()} {}

	OCREngine::OCREngine(
		const char* det_model_path,
		const char* det_param_path,
		const char* rec_model_path,
		const char* rec_param_path,
		const char* keys_path
	):
		det{det_model_path, det_param_path},
		rec{rec_model_path, rec_param_path, keys_path} {}


	OCREngine::OCREngine(Config& yaml):
		det{yaml.getDetModelPath().string(), yaml.getDetParamPath().string()},
		rec{yaml.getRecModelPath().string(), yaml.getRecParamPath().string(), yaml.getKeyPath().string()} {}


	std::vector<OCRResult> OCREngine::run(const std::string& image_path) const {
		// get image
		const cv::Mat image = cv::imread(image_path);
		return run(image);
	}


	std::vector<OCRResult> OCREngine::run(const cv::Mat& image) const {
		// det
		std::vector<TextRect> text_boxes = det.run(image);

		const std::size_t num_boxes = text_boxes.size();

		// get individual text images 4 rec
		const std::vector<cv::Mat> text_images = std::views::iota(std::size_t{0}, num_boxes)
		                                         | std::ranges::views::transform(
			                                         [&image, &text_boxes](const std::size_t i) -> cv::Mat {
				                                         return cropImage(image, text_boxes[i].rect);
			                                         }
		                                         )
		                                         | std::ranges::to<std::vector<cv::Mat> >();

		// skip cls cause who has the time for that

		// rec
		const std::vector<Text> text_lines = rec.run(text_images);

		// fill OCRResult vector
		const std::vector<OCRResult> results = std::views::iota(std::size_t{0}, num_boxes)
		                                       | std::ranges::views::transform(
			                                       [&text_boxes, &text_lines](const std::size_t i) -> OCRResult {
				                                       return OCRResult{
					                                       .rect = text_boxes[i],
					                                       .text = text_lines[i],
				                                       };
			                                       }
		                                       )
		                                       | std::ranges::to<std::vector<OCRResult> >();

		return results;
	}
} // ocr
