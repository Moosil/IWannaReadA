#pragma once

#include <qboxlayout.h>
#include <qmainwindow.h>
#include <qpushbutton.h>

#include "config.h"

namespace iwra {
	class ControlWindow : public QMainWindow {
	public:
		ControlWindow() = delete;

		ControlWindow(QWidget* parent, const std::shared_ptr<Config>& config);

		QWidget*     central_widget;
		QHBoxLayout* central_layout;
		QWidget*     control_parent_widget;
		QVBoxLayout* control_parent_layout;
		QPushButton* settings_button;
		QPushButton* select_area_button;
		QPushButton* toggle_popup_button;

		std::shared_ptr<Config> config;
	};
} // iwra
