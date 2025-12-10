//
// Created by rowan on 14/11/2025.
//

#define NOMINMAX // catching windows min max thingy
#include "rec.h"

#include <format>
#include <fstream>
#include <ranges>

#include <opencv2/core/mat.hpp>

#include "util.h"

namespace ocr {
	Rec::Rec(const std::string& det_model_path, const std::string& det_param_path, const std::string& keys_path) {
		init(det_model_path, det_param_path, keys_path);
	}

	void Rec::init(const std::string& det_model_path, const std::string& det_param_path, const std::string& keys_path) {
		std::string   line;
		std::ifstream file_stream{keys_path.c_str()};
		if (!file_stream.is_open()) {
			throw std::runtime_error(std::format("fatal error to open keys file at {}", keys_path));
		}
		while (std::getline(file_stream, line)) {
			keys.emplace_back(line);
		}

		net = std::make_unique<ncnn::Net>();
		net->load_param(det_param_path.c_str());
		net->load_model(det_model_path.c_str());
	}

	Rec::Rec(Rec&& other) noexcept :
		net{std::move(other.net)},
		keys{std::move(other.keys)} {
	}

	Rec& Rec::operator=(Rec&& other) noexcept {
		if (this != &other) {
			net  = std::move(other.net);
			keys = std::move(other.keys);
		}
		return *this;
	}

	std::vector<Text> Rec::run(const std::vector<cv::Mat>& images) const {
		const std::size_t length = images.size();
		std::vector<Text> text_lines{length};

		#pragma omp parallel for num_threads(10) schedule(dynamic)
		for (int i = 0; i < length; ++i) {
			text_lines[i] = _run(images[i]);
		}

		return text_lines;
	}

	Text Rec::_run(const cv::Mat& image) const {
		// resize image
		const float ratio = static_cast<float>(target_height) / static_cast<float>(image.rows);
		const int   rsz_w = static_cast<int>(static_cast<float>(image.cols) * ratio);

		ncnn::Mat in_inf = ncnn::Mat::from_pixels_resize(
			image.data,
			ncnn::Mat::PIXEL_RGB,
			image.cols,
			image.rows,
			rsz_w,
			target_height
		);
		in_inf.substract_mean_normalize(mean_values, norm_values);

		// inference
		ncnn::Extractor ex = net->create_extractor();
		ex.input("input", in_inf);
		ncnn::Mat out_inf;
		ex.extract("output", out_inf);

		return infer2Text(out_inf, {image.cols});
	}

	Text Rec::infer2Text(const ncnn::Mat& infer, const RetInfo info) const {
		const std::size_t cols = infer.w;
		if (cols != key_count) {
			return Text{};
		}

		std::string                            text;
		std::vector<float>                     text_scores;
		std::vector<std::pair<float, float>> text_lengths;

		std::size_t   prev_index  = -1;
		constexpr int blank_idx   = 0;
		const int     len         = infer.h;
		int           start_idx   = 0;
		int           blank_chain = 0;
		for (int i = 0; i < len; ++i) {
			const float* row_i   = infer.row(i);
			const auto   max_it  = std::max_element(row_i, row_i + cols);
			const size_t max_idx = std::distance(row_i, max_it);
			float        max_val = *max_it;

			// if index is same, collapse A A B _ B B -> A B _ B
			if (max_idx == prev_index) {
				continue;
			}

			// discard if max index is blank (0) A B _ B -> A B B
			if (max_idx == blank_idx) {
				blank_chain += 1;
				continue;
			}

			text.append(keys[max_idx]);
			text_scores.push_back(max_val);
			text_lengths.emplace_back(
				static_cast<float>(start_idx) / static_cast<float>(len),
				static_cast<float>(i) / static_cast<float>(len)
			);

			// update previous index
			prev_index  = max_idx;
			start_idx   = i;
			blank_chain = 0;
		}

		const float add = static_cast<float>(blank_chain) / static_cast<float>(len);
		for (auto& length : text_lengths) {
			length = {std::get<0>(length) + add, std::get<1>(length) + add};
		}
		if (!text_lengths.empty()) {
			text_lengths[0] = {std::get<0>(text_lengths[0]) - add, std::get<1>(text_lengths[0])};
		}

		return {
			.text = strip(text),
			.char_lengths = text_lengths,
			.scores = text_scores
		};
	}
} // ocr
