#include "det.h"


#include <algorithm>
#include <filesystem>
#include <numeric>
#include <ranges>

#include "util.h"
#include <opencv2/imgproc.hpp>


namespace ocr {
	Det::Det(const std::string& det_model_path, const std::string& det_param_path) {
		init(det_model_path, det_param_path);
	}

	void Det::init(const std::string& det_model_path, const std::string& det_param_path) {
		net = std::make_unique<ncnn::Net>();
		net->load_param(det_param_path.c_str());
		net->load_model(det_model_path.c_str());
	}

	Det::Det(Det&& other) noexcept :
		net{std::move(other.net)} {
	}

	Det& Det::operator=(Det&& other) noexcept {
		if (this != &other) {
			net = std::move(other.net);
		}
		return *this;
	}

	std::vector<TextRect> Det::run(const cv::Mat& image) const {
		// padding
		cv::Mat pad_image = image.clone();
		cv::copyMakeBorder(
			image,
			pad_image,
			padding,
			padding,
			padding,
			padding,
			cv::BORDER_CONSTANT | cv::BORDER_ISOLATED,
			cv::Scalar(255.f, 255.f, 255.f)
		);

		// resize
		const int target_size = std::min(
			max_side_len + 2 * padding,
			std::max(pad_image.rows, pad_image.cols)
		);

		const int   img_rows  = pad_image.rows,               img_cols  = pad_image.cols;
		const auto  img_rowsf = static_cast<float>(img_rows), img_colsf = static_cast<float>(img_cols);
		const float ratio     = static_cast<float>(target_size) / std::max(img_rowsf, img_colsf);
		const int   rsz_rows  = std::max(static_cast<int>(img_rowsf * ratio) / 32 * 32, 32); // rounding to nearest 32
		const int   rsz_cols  = std::max(static_cast<int>(img_colsf * ratio) / 32 * 32, 32); // rounding to nearest 32

		ncnn::Mat in_inf = ncnn::Mat::from_pixels_resize(
			pad_image.data,
			ncnn::Mat::PIXEL_RGB,
			img_cols,
			img_rows,
			rsz_cols,
			rsz_rows
		);
		in_inf.substract_mean_normalize(mean_values_, norm_values_);

		// inference: image -> Mat float
		ncnn::Extractor ex = net->create_extractor();
		ex.input("input", in_inf);
		ncnn::Mat out_inf;
		ex.extract("output", out_inf);

		// binarisation: Mat float -> Mat bool
		constexpr float denorm_values[1] = {255.f};
		out_inf.substract_mean_normalize(0, denorm_values);

		const cv::Mat pred(rsz_rows, rsz_cols, CV_8UC1);
		out_inf.to_pixels(pred.data, ncnn::Mat::PIXEL_GRAY);
		const cv::Mat bitmap = pred > threshold;


		return box_from_bitmap(pred, bitmap, img_cols, img_rows);
	}

	std::vector<TextRect> Det::box_from_bitmap(
		const cv::Mat& probability_map,
		const cv::Mat& bitmap,
		const int      dest_width,
		const int      dest_height
	) {
		const int width = bitmap.cols, height = bitmap.rows;

		// get contours of bitmap. as in strips that cover the "white" parts
		std::vector<std::vector<cv::Point> > contours;
		cv::findContours(bitmap, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

		std::vector<TextRect> output;
		for (const auto& contour : contours) {
			// get the smallest rect that covers the contour
			// if longest side is too small, continue
			cv::RotatedRect min_area_rect = cv::minAreaRect(contour);
			if (const float max_side_length = std::max(min_area_rect.size.width, min_area_rect.size.height);
				max_side_length < min_size) {
				continue;
			}

			// sorts min_area_rect's points to top left, top right, bottom right, bottom left
			Poly2F min_area_rect_points = rotatedRect2Poly2F(min_area_rect);

			// score particular box
			// if score lower than threshold, continue
			float score = box_score(probability_map, min_area_rect_points);
			if (score < box_threshold) {
				continue;
			}

			// unclip the text, increasing it's size to cover possibly cut out text
			// if too small or if inflating path fails, continue
			cv::RotatedRect unclip_rect = unclip(min_area_rect_points, unclip_ratio);
			if (unclip_rect.size.height <= 1 || unclip_rect.size.width <= 1) {
				continue;
			}

			// get the smallest rect that covers the contour
			// if too small, discard
			Poly2F unclip_rect_points = rotatedRect2Poly2F(unclip_rect);
			if (const float max_side_length = std::max(min_area_rect.size.width, min_area_rect.size.height);
				max_side_length < min_size + 2) {
				// 2 is arbitrary? (source: https://github.com/PaddlePaddle/PaddleOCR/blob/main/ppocr/postprocess/db_postprocess.py#L148)
				continue;
			}

			// resizing ratio when mapping bitmap -> destination
			const float ratio_col = static_cast<float>(dest_width) / static_cast<float>(width);
			const float ratio_row = static_cast<float>(dest_height) / static_cast<float>(height);

			// multiply points by ratio and clamp between 0 -> dest_width/height
			// copy -> (0..3 -> lambda) -> text_points
			Poly2I text_points;
			std::ranges::copy(
				std::views::iota(0, 4) | std::views::transform(
					[&unclip_rect_points, ratio_col, ratio_row, dest_width, dest_height](const int j) -> cv::Point {
						return {
							std::clamp(
								static_cast<int>(std::round(unclip_rect_points[j].x * ratio_col)) - padding,
								0,
								dest_width - 2 * padding - 1
							),
							std::clamp(
								static_cast<int>(std::round(unclip_rect_points[j].y * ratio_row)) - padding,
								0,
								dest_height - 2 * padding - 1
							)
						};
					}
				),
				text_points.begin()
			);

			output.emplace_back(text_points, score);
		}
		std::ranges::reverse(output);

		return output;
	}

	float Det::box_score(const cv::Mat& bitmap, const Poly2F& rect) {
		const int width = bitmap.cols, height = bitmap.rows;

		// get extent of rect, clamped 0 -> width/height
		// TODO check if working
		const int min_x = std::clamp(static_cast<int>(std::ceilf(std::max(rect[0].x, rect[3].x))), 0, width);
		const int max_x = std::clamp(static_cast<int>(std::ceilf(std::max(rect[1].x, rect[2].x))), 0, width);
		const int min_y = std::clamp(static_cast<int>(std::ceilf(std::max(rect[0].y, rect[1].y))), 0, height);
		const int max_y = std::clamp(static_cast<int>(std::ceilf(std::max(rect[2].y, rect[3].y))), 0, height);

		// zero initialised mask size of extent of rect
		cv::Mat mask = cv::Mat::zeros(max_y - min_y + 1, max_x - min_x + 1, CV_8UC1);

		// rect moved so bottom left point is at 0,0
		Poly2I normalised_rect{
			cv::Point{static_cast<int>(rect[0].x) - min_x, static_cast<int>(rect[0].y) - min_y},
			cv::Point{static_cast<int>(rect[1].x) - min_x, static_cast<int>(rect[1].y) - min_y},
			cv::Point{static_cast<int>(rect[2].x) - min_x, static_cast<int>(rect[2].y) - min_y},
			cv::Point{static_cast<int>(rect[3].x) - min_x, static_cast<int>(rect[3].y) - min_y}
		};

		// fill the mask
		const cv::Point* pts[1] = {normalised_rect.data()};
		constexpr int    npt[]  = {4};
		cv::fillPoly(mask, pts, npt, 1, cv::Scalar(1));

		// gets mean of bitmap & mask
		const cv::Mat croppedImage = bitmap(
			cv::Rect(
				min_x,
				min_y,
				max_x - min_x + 1,
				max_y - min_y + 1
			)
		).clone();

		return static_cast<float>(cv::mean(croppedImage, mask)[0] / 255.f);
	}
} // ocr
