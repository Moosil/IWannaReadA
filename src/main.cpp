#define NOMINMAX
#include <ranges>
#include <spdlog/spdlog.h>
#include <Windows.h>

#include "ocr_engine.h"
#include "screenshot.h"
#include "tooltip.h"

using namespace ocr;

[[noreturn]] int main() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	spdlog::set_pattern("%v");

	const auto engine = OCREngine(
		"../config.yaml"
	);

	if (RegisterHotKey(
		nullptr,
		1,
		MOD_NOREPEAT,
		VK_SNAPSHOT
	)) {
		spdlog::info("hotkey registered successfully");
	}

	MSG                            msg = {nullptr};
	std::unique_ptr<ScreenshotWnd> ss_wnd;
	std::unique_ptr<TooltipWnd>    tt_wnd;
	cv::Mat                        ss;
	cv::Point                      topleft;
	while (true) {
		if (ss_wnd) {
			ss_wnd->update();

			if (!ss_wnd->is_running) {
				const auto output = engine.run(ss);
				ss_wnd            = nullptr;
				spdlog::info("top left: {}, {}", topleft.x, topleft.y);
				tt_wnd = TooltipWnd::initTooltip(output, topleft, "");
			}
		} else {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_HOTKEY) {
					spdlog::info("hotkey fired");
					if (!ss_wnd) {
						if (tt_wnd) {
							DestroyWindow(tt_wnd->hwnd);
							tt_wnd = nullptr;
						}
						ss_wnd = ScreenshotWnd::startScreenShot(&ss, &topleft);
					}
				}
			}

			if (tt_wnd) {
				tt_wnd->updateLoop();
			}
		}
	}
}
