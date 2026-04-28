#pragma once


#include <windows.h>
#include <spdlog/spdlog.h>

#include <string>
#include <source_location>


namespace iwra {
	enum class ERR_LEVEL {
		WARN = 0,
		FATAL,
	};

	void log(HRESULT err, std::string function_name, ERR_LEVEL err_level = ERR_LEVEL::WARN);

	void log(BOOL ret, std::string function_name, ERR_LEVEL err_level = ERR_LEVEL::WARN);

	static std::chrono::time_point<std::chrono::steady_clock> start_time;
	static std::source_location                               start_location;

	void startTimeFunction(const std::source_location& location = std::source_location::current());

	void endTimeFunction(const std::source_location& location = std::source_location::current());
}
