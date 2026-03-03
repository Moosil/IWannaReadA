#include "util.h"

#include <d2d1helper.h>
#include <dwrite.h>
#include <shellscalingapi.h>
#include <windows.h>
#include <opencv2/imgproc.hpp>
#pragma comment(lib,"Shcore.lib")
#include <fstream>
#include <filesystem>
#include <ranges>
#include <utf8/cpp20.h>
#include <wrl/client.h>

#include "log.h"


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

	Clipper2Lib::Path64 rect2path(const Poly2I& rect) {
		return {
			Clipper2Lib::Point64(rect[0].x, rect[0].y),
			Clipper2Lib::Point64(rect[1].x, rect[1].y),
			Clipper2Lib::Point64(rect[2].x, rect[2].y),
			Clipper2Lib::Point64(rect[3].x, rect[3].y)
		};
	}

	Clipper2Lib::Path64 rect2path(const Poly2F& rect) {
		return {
			Clipper2Lib::Point64(rect[0].x, rect[0].y),
			Clipper2Lib::Point64(rect[1].x, rect[1].y),
			Clipper2Lib::Point64(rect[2].x, rect[2].y),
			Clipper2Lib::Point64(rect[3].x, rect[3].y)
		};
	}

	cv::RotatedRect unclip(const Poly2F& rect, const float unclip_ratio) {
		// get unclip distance (don't fully understand what it's doing yet?)
		const float distance = getUnclipDistance(rect, unclip_ratio);

		// convert rect to Clipper2 path
		const Clipper2Lib::Paths64 path = {rect2path(rect)};

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
			area      += rect[i].x * rect[i + 1].y - rect[i].y * rect[i + 1].x;
			perimeter += distance(rect[i], rect[i + 1]);
		}
		area      += rect[max_index].x * rect[0].y - rect[max_index].y * rect[0].x;
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

		// get width and height of rectangle when it's not rotated
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

		// if text is vertical, rotate it
		if (static_cast<float>(text_image.rows) >= static_cast<float>(text_image.cols) * 1.5f) {
			cv::Mat dst;
			cv::rotate(text_image, dst, cv::ROTATE_90_COUNTERCLOCKWISE);
			return dst;
		}
		return text_image;
	}

	std::pair<int, int> getMonitorDPI() {
		HMONITOR      hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
		UINT          dpi_x, dpi_y;
		const HRESULT err = GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
		log(err, "GetDpiForMonitor", ERR_LEVEL::WARN);
		return {dpi_x, dpi_y};
	}

	std::pair<int, int> getScreenSize() {
		const auto [dpi_x, dpi_y] = getMonitorDPI();
		return {GetSystemMetricsForDpi(SM_CXSCREEN, dpi_x), GetSystemMetricsForDpi(SM_CYSCREEN, dpi_y)};
	}

	int lerpi(const int a, const int b, const float t) {
		return static_cast<int>(std::round(std::lerp(static_cast<float>(a), static_cast<float>(b), t)));
	}

	std::string& ltrim(std::string& str) {
		const auto it2 = std::ranges::find_if(
			str,
			[](const char ch) {
				return !(std::isspace<char>(ch, std::locale::classic()) || ch == '\0');
			}
		);
		str.erase(str.begin(), it2);
		return str;
	}

	std::string& rtrim(std::string& str) {
		const auto it1 = std::find_if(
			str.rbegin(),
			str.rend(),
			[](const char ch) {
				return !(std::isspace<char>(ch, std::locale::classic()) || ch == '\0');
			}
		);
		str.erase(it1.base(), str.end());
		return str;
	}

	std::string& trim(std::string& str) {
		return ltrim(rtrim(str));
	}

	std::string trim_copy(const std::string& str) {
		auto s = str;
		return ltrim(rtrim(s));
	}

	std::wstring utf8ToWide(const std::string& str) {
		const std::u16string u16str = utf8::utf8to16(str);
		const auto           wcstr  = reinterpret_cast<const wchar_t*>(u16str.c_str());
		return wcstr;
	}

	std::string wideToUtf8(const std::wstring& wstr) {
		const std::u16string u16str = reinterpret_cast<const char16_t*>(wstr.c_str());
		const auto           str    = utf8::utf16to8(u16str);
		return str;
	}

	std::pair<float, float> getTextSize(
		const std::wstring&                              text,
		const Microsoft::WRL::ComPtr<IDWriteTextFormat>& direct_write_text_format,
		const Microsoft::WRL::ComPtr<IDWriteFactory>&    direct_write_factory
	) {
		Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
		HRESULT                                   err = direct_write_factory->CreateTextLayout(
			text.c_str(),
			static_cast<UINT32>(text.length()),
			direct_write_text_format.Get(),
			100,
			100,
			&text_layout
		);
		log(err, "IDWriteFactory::CreateTextLayout", ERR_LEVEL::WARN);

		DWRITE_TEXT_METRICS text_metrics{};
		err = text_layout->GetMetrics(&text_metrics);
		log(err, "IDWriteTextLayout::GetMetrics", ERR_LEVEL::WARN);
		text_layout->Release();

		return {text_metrics.width, text_metrics.height};
	}

	std::string readFile(const std::filesystem::path& path) {
		auto&& file = std::ifstream(path, std::ios::in | std::ios::binary);
		file >> std::noskipws;
		auto&& view = std::views::istream<char>(file);
		return std::ranges::to<std::string>(view);
	}
}
