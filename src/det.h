#pragma once

#include "common.h"
#include "ncnn/net.h"

namespace ocr {
	class Det {
	private:
		static constexpr std::size_t max_candidates = 1000;
		static constexpr float       threshold{.3f};
		static constexpr float       box_threshold{.7f};
		static constexpr float       min_size{3};
		static constexpr float       unclip_ratio{2};
		static constexpr int         padding{50};
		static constexpr int         max_side_len{1024};
		static constexpr float       mean_values_[3]{.485f * 255.f, .456f * 255.f, .406f * 255.f};
		static constexpr float       norm_values_[3]{1.f / .229f / 255.f, 1.f / .224f / 255.f, 1.f / .225f / 255.f};

		std::unique_ptr<ncnn::Net> net{};

		static std::vector<TextRect> box_from_bitmap(
			const cv::Mat& probability_map,
			const cv::Mat& bitmap,
			int            dest_width,
			int            dest_height
		);

		static float box_score(const cv::Mat& bitmap, const Poly2F& rect);

	public:
		Det() = default;

		Det(const std::string& det_model_path, const std::string& det_param_path);

		void init(const std::string& det_model_path, const std::string& det_param_path);

		Det(Det&& other) noexcept;

		Det& operator =(Det&& other) noexcept;

		Det(const Det&) = delete;

		Det& operator =(const Det&) = delete;

		[[nodiscard]] std::vector<TextRect> run(const cv::Mat& image) const;
	};
}
