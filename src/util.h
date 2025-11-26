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
} // ocr
