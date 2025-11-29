//
// Created by rowan on 13/11/2025.
//

#pragma once
#include <clipper2/clipper.h>
#include <opencv2/core/types.hpp>
#include <spdlog/spdlog.h>

#include "common.h"

namespace ocr {
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

	Clipper2Lib::Path64 rect2path(const Poly2I& rect);

	Clipper2Lib::Path64 rect2path(const Poly2F& rect);

	cv::RotatedRect unclip(const Poly2F& rect, float unclip_ratio);

	float distance(cv::Point2f a, cv::Point2f b);

	float getUnclipDistance(const Poly2F& rect, float unclip_ratio);

	cv::Mat cropImage(const cv::Mat& image, Poly2I rect);

	std::string strip(std::string& text);

	std::pair<unsigned long, unsigned long> getScreenSize();

	int lerpi(int a, int b, float t);

	// Source - https://stackoverflow.com/a
	// Posted by g-217, modified by community. See post 'Timeline' for change history
	// Retrieved 2025-11-27, License - CC BY-SA 3.0
	std::string& ltrim(std::string& str);

	std::string& rtrim(std::string& str);

	std::string& trim(std::string& str);

	std::string trim_copy(const std::string& str);

	// End attribution
} // ocr
