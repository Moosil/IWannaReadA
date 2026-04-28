#pragma once


#include <string>


namespace iwra {
	// Source - https://stackoverflow.com/a
	// Posted by g-217, modified by community. See post 'Timeline' for change history
	// Retrieved 2025-11-27, License - CC BY-SA 3.0
	std::string& ltrim(std::string& str);

	std::string& rtrim(std::string& str);

	std::string& trim(std::string& str);

	std::string trim_copy(const std::string& str);
	// End attribution

	std::wstring utf8ToWide(const std::string& str);

	std::string wideToUtf8(const std::wstring& wstr);
}