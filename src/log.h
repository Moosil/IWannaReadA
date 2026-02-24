#pragma once
#include <string>
#include <spdlog/spdlog.h>

namespace ocr {
	enum class ERR_LEVEL {
		WARN = 0,
		FATAL,
	};
	inline void log(HRESULT err, std::string function_name, const ERR_LEVEL err_level = ERR_LEVEL::WARN) {
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
	inline void log(const BOOL ret, std::string function_name, const ERR_LEVEL err_level = ERR_LEVEL::WARN) {
		if (ret) {
			// spdlog::info("{} called SUCCEEDED, HRESULT = 0x{:X}", function_name, err);
		} else {
			if (err_level == ERR_LEVEL::WARN) {
				spdlog::warn("{} called FAILED, DWORD = 0x{:X}", function_name, GetLastError());
			} else {
				spdlog::critical("{} called FAILED, DWORD = 0x{:X}", function_name, GetLastError());
			}
		}
	}
}
