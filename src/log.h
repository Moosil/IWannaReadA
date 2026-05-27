#pragma once

#include <chrono>
#include <source_location>

namespace iwra {
	static std::chrono::time_point<std::chrono::steady_clock> start_time;
	static std::source_location                               start_location;

	void startTimeFunction(const std::source_location& location = std::source_location::current());

	void endTimeFunction(const std::source_location& location = std::source_location::current());
}
