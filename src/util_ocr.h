#pragma once


#include <clipper2/clipper.h>
#include <opencv2/core/types.hpp>
#include <spdlog/spdlog.h>

#include "common.h"


namespace iwra {
	Poly2F rotatedRect2Poly2F(const cv::RotatedRect& rect);

	template<std::ranges::input_range Range0, std::ranges::input_range Range1>
		requires std::same_as<std::ranges::range_value_t<Range0>, cv::Point> &&
		         std::same_as<std::ranges::range_value_t<Range1>, cv::Point>
	bool intersects(const Range0& range0, const Range1& range1) {
		Clipper2Lib::Path64 path0;
		for (const auto& [x, y] : range0) {
			path0.emplace_back(x, y);
		}
		const Clipper2Lib::Paths64 paths0{path0};

		Clipper2Lib::Path64 path1;
		for (const auto& [x, y] : range1) {
			path1.emplace_back(x, y);
		}
		const Clipper2Lib::Paths64 paths1{path1};

		const auto intersection_paths = Clipper2Lib::Intersect(paths0, paths1, Clipper2Lib::FillRule::NonZero);
		return !intersection_paths.empty();
	}

	template<std::ranges::input_range Range0, std::ranges::input_range Range1>
		requires std::same_as<std::ranges::range_value_t<Range0>, cv::Point> &&
		         std::same_as<std::ranges::range_value_t<Range1>, cv::Point>
	std::vector<cv::Point> union_(const Range0& range0, const Range1& range1) {
		Clipper2Lib::Path64 path0;
		for (const auto& [x, y] : range0) {
			path0.emplace_back(x, y);
		}
		const Clipper2Lib::Paths64 paths0{path0};

		Clipper2Lib::Path64 path1;
		for (const auto& [x, y] : range1) {
			path1.emplace_back(x, y);
		}
		const Clipper2Lib::Paths64 paths1{path1};

		const auto             union_paths = Clipper2Lib::Union(paths0, paths1, Clipper2Lib::FillRule::NonZero);
		std::vector<cv::Point> res;
		if (union_paths.size() != 1) {
			spdlog::warn("union_ failed");
			return {};
		}
		const auto& union_path = union_paths[0];
		for (const auto& [x, y] : union_path) {
			res.emplace_back(static_cast<int>(x), static_cast<int>(y));
		}
		res.emplace_back(static_cast<int>(union_path[0].x), static_cast<int>(union_path[0].y));
		return res;
	}

	inline Clipper2Lib::Path64 rect2path(const Poly2I& rect) {
		return {
			Clipper2Lib::Point64(rect[0].x, rect[0].y),
			Clipper2Lib::Point64(rect[1].x, rect[1].y),
			Clipper2Lib::Point64(rect[2].x, rect[2].y),
			Clipper2Lib::Point64(rect[3].x, rect[3].y)
		};
	}

	inline Clipper2Lib::Path64 rect2path(const Poly2F& rect) {
		return {
			Clipper2Lib::Point64(rect[0].x, rect[0].y),
			Clipper2Lib::Point64(rect[1].x, rect[1].y),
			Clipper2Lib::Point64(rect[2].x, rect[2].y),
			Clipper2Lib::Point64(rect[3].x, rect[3].y)
		};
	}

	cv::RotatedRect unclip(const Poly2F& rect, float unclip_ratio);

	inline float distance(const cv::Point2f a, const cv::Point2f b) {
		const auto c = b - a;
		return std::sqrt(c.x * c.x + c.y * c.y);
	}

	float getUnclipDistance(const Poly2F& rect, float unclip_ratio);

	cv::Mat cropImage(const cv::Mat& image, Poly2I rect);
}