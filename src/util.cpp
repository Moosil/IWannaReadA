#include "util.h"


#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib,"Shcore.lib")

#include <fstream>
#include <ranges>'

#include "log.h"


std::pair<int, int> iwra::getMonitorDPI() {
	const HMONITOR      hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
	UINT          dpi_x, dpi_y;
	const HRESULT err = GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
	log(err, "GetDpiForMonitor", ERR_LEVEL::WARN);
	return {dpi_x, dpi_y};
}

std::pair<int, int> iwra::getScreenSize() {
	const auto [dpi_x, dpi_y] = getMonitorDPI();
	return {GetSystemMetricsForDpi(SM_CXSCREEN, dpi_x), GetSystemMetricsForDpi(SM_CYSCREEN, dpi_y)};
}

std::string iwra::readFile(const std::filesystem::path& path) {
	auto&& file = std::ifstream(path, std::ios::in | std::ios::binary);
	file >> std::noskipws;
	auto&& view = std::views::istream<char>(file);
	return std::ranges::to<std::string>(view);

}
