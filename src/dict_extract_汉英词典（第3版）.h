#pragma once

#include "dict_extract.h"

namespace ocr {
	class 汉英词典第三版Extractor : public DictExtractor {
	public:
		[[nodiscard]] std::vector<EntryInfo> extractMDictHTML(const std::string& html) const override;
	};
}