#include "log.h"


void iwra::log(HRESULT err, std::string function_name, const ERR_LEVEL err_level) {
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

void iwra::log(const BOOL ret, std::string function_name, const ERR_LEVEL err_level) {
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

void iwra::startTimeFunction(const std::source_location& location) {
	start_time     = std::chrono::steady_clock::now();
	start_location = location;
}

void iwra::endTimeFunction(const std::source_location& location) {
	const auto end = std::chrono::steady_clock::now();
	spdlog::info(
		"({})->({})  ran in {}ms",
		start_location.function_name(),
		location.function_name(),
		std::chrono::duration<double, std::milli>(end - start_time).count()
	);
}