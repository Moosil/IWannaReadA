#pragma once

#include "common.h"

#include <net.h>

namespace iwra {
	class Det {
	private:
		static constexpr std::size_t maxCandidates = 1000;
		static constexpr float       bitmapThreshold{.3f};
		static constexpr float       boxThreshold{.7f};
		static constexpr float       minSize{3};
		static constexpr float       unclipRatio{2};
		static constexpr int         padding{50};
		static constexpr int         maxSideLen{1024};
		static constexpr float       meanValues[3]{.485f * 255.f, .456f * 255.f, .406f * 255.f};
		static constexpr float       normValues[3]{1.f / .229f / 255.f, 1.f / .224f / 255.f, 1.f / .225f / 255.f};

		std::unique_ptr<ncnn::Net> net{};

		static std::vector<TextRect> boxFromBitmap(
			const cv::Mat& probability_map,
			const cv::Mat& bitmap,
			int            dest_width,
			int            dest_height
		);

		static float boxScore(const cv::Mat& bitmap, const Poly2F& rect);

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
