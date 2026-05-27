#pragma once

#include <qguiapplication.h>
#include <qscreen.h>

namespace iwra {
	inline std::pair<int, int> getScreenSize() {
		const QSize screen_size = QGuiApplication::primaryScreen()->size();
		return {screen_size.width(), screen_size.height()};
	}
}

