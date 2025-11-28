//
// Created by rowan on 13/11/2025.
//

#pragma once
#include <opencv2/core/types.hpp>

#include "common.h"

namespace ocr {
	Poly2F rotatedRect2Poly2F(const cv::RotatedRect& rect);

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
