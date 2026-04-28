#pragma once

#include <string>
#include <chrono>
#include <filesystem>


struct IDWriteTextFormat;
struct IDWriteFactory;

namespace iwra {
	std::pair<int, int> getMonitorDPI();

	std::pair<int, int> getScreenSize();

	std::string pinyinNumberToTone(const std::string& in_pinyin);

	std::string readFile(const std::filesystem::path& path);
} // ocr
