#pragma once

#include <string>

namespace iwra {
	// Source - https://stackoverflow.com/a
	// Posted by g-217, modified by community. See post 'Timeline' for change history
	// Retrieved 2025-11-27, License - CC BY-SA 3.0
	std::string& ltrim(std::string& str);

	std::string& rtrim(std::string& str);

	inline std::string& trim(std::string& str) {
		return ltrim(rtrim(str));
	}

	inline std::string trimCopy(const std::string& str) {
		auto s = str;
		return ltrim(rtrim(s));
	}

	// End attribution
}
