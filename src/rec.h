//
// Created by rowan on 14/11/2025.
//

#pragma once
#include <memory>
#include <vector>

#include "common.h"
#include "ncnn/net.h"


namespace ocr {
	class Rec {
	private:
		struct RetInfo {
			int width;
		};


		static constexpr int target_height = 48;
		static constexpr float mean_values[3]{127.5f, 127.5f, 127.5f};
		static constexpr float norm_values[3]{1.0f / 127.5f, 1.0f / 127.5f, 1.0f / 127.5f};
		static constexpr std::size_t key_count = 18385;

		std::unique_ptr<ncnn::Net> net{};

		std::vector<std::string> keys{};

		[[nodiscard]] Text _run(const cv::Mat& image) const;

		[[nodiscard]] Text infer2Text(const ncnn::Mat& infer, RetInfo info) const;
	public:
		Rec() = default;
		Rec(const std::string& det_model_path, const std::string& det_param_path, const std::string& keys_path);
		void init(const std::string& det_model_path, const std::string& det_param_path, const std::string& keys_path);

		Rec(Rec&& other) noexcept;
		Rec& operator = (Rec&& other) noexcept;
		Rec(const Rec &) = delete;
		Rec& operator = (const Rec&) = delete;

		[[nodiscard]] std::vector<Text> run(const std::vector<cv::Mat>& images) const;
	};
} // ocr