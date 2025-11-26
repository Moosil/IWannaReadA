//
// Created by rowan on 25/11/2025.
//

#pragma once
#include <intsafe.h>
#include <string>
#include <spdlog/spdlog.h>

namespace ocr {
	enum class ERR_LEVEL {
		WARN = 0,
		FATAL,
	};
	inline void log(HRESULT err, std::string function_name, ERR_LEVEL err_level = ERR_LEVEL::WARN) {
		if (SUCCEEDED(err)) {
			// spdlog::info("{} called SUCCEEDED, HRESULT = 0x{:X}", function_name, err);
		} else {
			if (err_level == ERR_LEVEL::WARN) {
				spdlog::warn("{} called FAILED, HRESULT = 0x{:X}", function_name, err);
			} else {
				spdlog::critical("{} called FAILED, HRESULT = 0x{:X}", function_name, err);
			}
		}
	}
}
