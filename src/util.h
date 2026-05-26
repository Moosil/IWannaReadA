#pragma once

#include <qguiapplication.h>
#include <qscreen.h>


namespace iwra {
	inline std::pair<int, int> getScreenSize() {
		const QSize screenSize = QGuiApplication::primaryScreen()->size();
		return {screenSize.width(), screenSize.height()};
	}
}

