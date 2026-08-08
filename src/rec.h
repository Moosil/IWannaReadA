#pragma once

#include <memory>
#include <vector>
#include <net.h>

#include "common.h"

namespace iwra {
	class Rec {
	private:
		static constexpr int         targetHeight = 48;
		static constexpr float       meanValues[3]{127.5f, 127.5f, 127.5f};
		static constexpr float       normValues[3]{1.0f / 127.5f, 1.0f / 127.5f, 1.0f / 127.5f};

		std::unique_ptr<ncnn::Net> net{};

		std::vector<std::string> keys{};

		[[nodiscard]] Text runSingle(const cv::Mat& image) const;

		[[nodiscard]] Text infer2Text(const ncnn::Mat& infer) const;

	public:
		Rec() = default;

		Rec(const std::string& det_model_path, const std::string& det_param_path, const std::string& keys_path);

		void init(const std::string& det_model_path, const std::string& det_param_path, const std::string& keys_path);

		Rec(Rec&& other) noexcept;

		Rec& operator =(Rec&& other) noexcept;

		Rec(const Rec&) = delete;

		Rec& operator =(const Rec&) = delete;

		[[nodiscard]] std::vector<Text> run(const std::vector<cv::Mat>& images) const;
	};
} // ocr
