//
// Created by rowan on 13/11/2025.
//

#include "util.h"

#include "clipper2/clipper.h"
#include <opencv2/imgproc.hpp>
#include <Windows.h>


namespace ocr {
	Poly2F rotatedRect2Poly2F(const cv::RotatedRect& rect) {
		Poly2F poly{};
		rect.points(poly.data());
		std::ranges::sort(poly, [](const cv::Point2f& lhs, const cv::Point2f& rhs) { return lhs.x < rhs.x; });

		// sorts points top left, top right, bottom right, bottom left
		if (poly[1].y > poly[0].y) {
			if (poly[3].y > poly[2].y) {
				std::rotate(poly.begin() + 1, poly.begin() + 2, poly.end()); // [ 0, 1, 2, 3 ] -> [ 0, 2, 3, 1 ]
			} else {
				std::swap(poly[1], poly[3]); // [ 0, 1, 2, 3 ] -> [ 0, 3, 2, 1 ]
			}
		} else {
			if (poly[3].y > poly[2].y) {
				std::ranges::rotate(poly, poly.begin() + 1); // [ 0, 1, 2, 3 ] -> [ 1, 2, 3, 0 ]
			} else {
				std::swap(poly[0], poly[1]); // [ 0, 1, 2, 3 ] -> [ 1, 0, 2, 3 ]
				std::swap(poly[1], poly[3]); // [ 1, 0, 2, 3 ] -> [ 1, 3, 2, 0 ]
			}
		}

		return poly;
	}

	cv::RotatedRect unclip(const Poly2F& rect, const float unclip_ratio) {
		// get unclip distance (don't fully understand what it's doing yet?)
		const float distance = getUnclipDistance(rect, unclip_ratio);

		// convert rect to Clipper2 path
		const Clipper2Lib::Paths64 path{
			{
				Clipper2Lib::Point64(rect[0].x, rect[0].y),
				Clipper2Lib::Point64(rect[1].x, rect[1].y),
				Clipper2Lib::Point64(rect[2].x, rect[2].y),
				Clipper2Lib::Point64(rect[3].x, rect[3].y)
			}
		};

		const Clipper2Lib::Paths64 inflated_path = Clipper2Lib::InflatePaths(
			path,
			distance,
			Clipper2Lib::JoinType::Round,
			Clipper2Lib::EndType::Polygon
		);

		// convert Clipper2 path to vector of opencv point (float)
		std::vector<cv::Point2f> points;
		for (const auto& sol_path : inflated_path) {
			for (const auto& pt : sol_path) {
				points.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
			}
		}

		if (points.empty()) {
			// IDK if this can happen tbh...
			return {cv::Point2f(0.f, 0.f), cv::Size2f(1.f, 1.f), 0.f};
			// ReSharper disable once CppRedundantElseKeywordInsideCompoundStatement
		} else {
			return cv::minAreaRect(points);
		}
	}

	float distance(const cv::Point2f a, const cv::Point2f b) {
		const auto c = b - a;
		return std::sqrt(c.x * c.x + c.y * c.y);
	}

	float getUnclipDistance(const Poly2F& rect, const float unclip_ratio) {
		float                 area      = 0, perimeter = 0;
		constexpr std::size_t max_index = 3;

		// Shoelace formula start (https://en.wikipedia.org/wiki/Shoelace_formula)[Wikipedia]
		for (std::size_t i = 0; i < max_index; i++) {
			area += rect[i].x * rect[i + 1].y - rect[i].y * rect[i + 1].x;
			perimeter += distance(rect[i], rect[i + 1]);
		}
		area += rect[max_index].x * rect[0].y - rect[max_index].y * rect[0].x;
		perimeter += distance(rect[max_index], rect[0]);

		area = std::abs(area / 2.f);
		// Shoelace formula end

		if (perimeter < 1e-6) {
			return 0;
		}
		return area * unclip_ratio / perimeter;
	}

	cv::Mat cropImage(const cv::Mat& image, Poly2I rect) {
		auto          [left, right] = std::minmax({rect[0].x, rect[1].x, rect[2].x, rect[3].x});
		auto          [bottom, top] = std::minmax({rect[0].y, rect[1].y, rect[2].y, rect[3].y});
		const cv::Mat crop_image    = image(cv::Rect(left, bottom, right - left, top - bottom)).clone();

		for (auto& point : rect) {
			point.x -= left;
			point.y -= bottom;
		}

		// get width and height of rectangle when its not rotated
		const float crop_w = distance(rect[0], rect[1]);
		const float crop_h = distance(rect[0], rect[3]);

		// what the image is
		const std::vector src_rect = {
			cv::Point2f(rect[0]),
			cv::Point2f(rect[1]),
			cv::Point2f(rect[2]),
			cv::Point2f(rect[3])
		};

		// what we want the image to be transformed to
		const std::vector dst_rect = {
			cv::Point2f(0.f, 0.f),
			cv::Point2f(crop_w, 0.f),
			cv::Point2f(crop_w, crop_h),
			cv::Point2f(0.f, crop_h)
		};

		// TODO benchmark different solve methods
		const cv::Mat transform_mat = cv::getPerspectiveTransform(src_rect, dst_rect, cv::DECOMP_LU);

		// transform image according to transformation matrix
		cv::Mat text_image;
		cv::warpPerspective(
			crop_image,
			text_image,
			transform_mat,
			cv::Size(static_cast<int>(crop_w), static_cast<int>(crop_h)),
			cv::BORDER_REPLICATE
		);

		// TODO: we can know this earlier (dst_rect)
		// if text is vertical, rotate it
		if (static_cast<float>(text_image.rows) >= static_cast<float>(text_image.cols) * 1.5f) {
			cv::Mat dst;
			// TODO check if correct
			cv::rotate(text_image, dst, cv::ROTATE_90_COUNTERCLOCKWISE);
			return dst;
		}
		return text_image;
	}

	std::string strip(std::string& text) {
		std::locale loc;
		std::erase_if(
			text,
			[loc](const unsigned char ch) -> bool {
				return std::isspace(ch, loc);
			}
		);
		return text;
	}

	std::tuple<unsigned long, unsigned long> getScreenSize() {
		DEVMODE devMode{};
		devMode.dmSize = sizeof(DEVMODEA);
		EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);
		return {devMode.dmPelsWidth, devMode.dmPelsHeight};
	}

	std::string utf8_to_gbk(const std::string& utf8) {
		// UTF-8 → UTF-16
		const int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		std::wstring wstr(wlen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wlen);

		// UTF-16 → GBK (936)
		const int len = WideCharToMultiByte(936, 0, wstr.c_str(), -1, nullptr, 0, NULL, NULL);
		std::string gbk(len, '\0');
		WideCharToMultiByte(936, 0, wstr.c_str(), -1, &gbk[0], len, NULL, NULL);

		gbk.pop_back(); // remove null
		return gbk;
	}

	std::wstring utf8_to_utf16(const std::string& str) {
		const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
		wstr.pop_back(); // remove extra null
		return wstr;
	}
}