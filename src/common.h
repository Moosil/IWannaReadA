#pragma once

#include <array>
#include <format>
#include <opencv2/core/types.hpp>

namespace ocr {
	using Poly2I = std::array<cv::Point, 4>;
	using Poly2F = std::array<cv::Point2f, 4>;

	inline const cv::Point& getTopLeft(const Poly2I& poly) { return poly[0]; }
	inline const cv::Point& getTopRight(const Poly2I& poly) { return poly[1]; }
	inline const cv::Point& getBottomRight(const Poly2I& poly) { return poly[2]; }
	inline const cv::Point& getBottomLeft(const Poly2I& poly) { return poly[3]; }

	inline int getTop(const Poly2I& poly) { return std::min(getTopLeft(poly).y, getTopRight(poly).y); }
	inline int getRight(const Poly2I& poly) { return std::max(getTopRight(poly).x, getBottomRight(poly).x); }
	inline int getBottom(const Poly2I& poly) { return std::max(getBottomLeft(poly).y, getBottomRight(poly).y); }
	inline int getLeft(const Poly2I& poly) { return std::min(getTopLeft(poly).x, getBottomLeft(poly).x); }

	struct TextRect {
		Poly2I rect;
		float  score{};

		// Convert to std::string
		explicit operator std::string() const {
			return std::format(
				"TextRect{{\n\trect=[\n\t\t({}, {}),\n\t\t({}, {}),\n\t\t({}, {}),\n\t\t({}, {})\n\t],\n\tscore={}\n}}",
				rect[0].x,
				rect[0].y,
				rect[1].x,
				rect[1].y,
				rect[2].x,
				rect[2].y,
				rect[3].x,
				rect[3].y,
				score
			);
		}
	};

	struct Text {
		std::string                            text;
		std::vector<std::pair<float, float> > char_lengths;
		std::vector<float>                     scores;

		// Convert to std::string
		explicit operator std::string() const {
			return std::format("Text{{\n\ttext={},\n\tscore={}\n}}", text, scores);
		}
	};

	struct OCRResult {
		TextRect rect;
		Text     text;

		// Convert to std::string
		explicit operator std::string() const {
			return std::format("OCRResult{{\nrect={},\ntext={}\n}}", std::string(rect), std::string(text));
		}
	};

	struct OCRResultPacked {
		Poly2I      rect;
		std::string text;
	};
}
