#include "util.h"

#include <fstream>
#include <qguiapplication.h>
#include <qscreen.h>
#include <ranges>

#include "log.h"


std::pair<int, int> iwra::getScreenSize() {
	const QSize screenSize = QGuiApplication::primaryScreen()->size();
	return {screenSize.width(), screenSize.height()};
}

std::string iwra::readFile(const std::filesystem::path& path) {
	auto&& file = std::ifstream(path, std::ios::in | std::ios::binary);
	file >> std::noskipws;
	auto&& view = std::views::istream<char>(file);
	return std::ranges::to<std::string>(view);
}
