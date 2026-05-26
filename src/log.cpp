#include "log.h"

#include <spdlog/spdlog.h>

namespace iwra {
	void startTimeFunction(const std::source_location& location) {
		start_time     = std::chrono::steady_clock::now();
		start_location = location;
	}

	void endTimeFunction(const std::source_location& location) {
		const auto end = std::chrono::steady_clock::now();
		spdlog::info(
			"({})->({})  ran in {}ms",
			start_location.function_name(),
			location.function_name(),
			std::chrono::duration<double, std::milli>(end - start_time).count()
		);
	}
}
