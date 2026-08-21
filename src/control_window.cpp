#include "control_window.h"

#include <qboxlayout.h>

iwra::ControlWindow::ControlWindow(QWidget* parent, const std::shared_ptr<Config>& config):
	QMainWindow(parent),
	config{config},
	central_widget{new QWidget(this)},
	central_layout{new QHBoxLayout},
	control_parent_widget{new QWidget(central_widget)},
	control_parent_layout{new QVBoxLayout},
	settings_button{new QPushButton("Settings", central_widget)},
	select_area_button{new QPushButton("Select Area", control_parent_widget)},
	toggle_popup_button{new QPushButton("Toggle Popup", control_parent_widget)}
{
	this->setCentralWidget(central_widget);
	central_widget->setLayout(central_layout);
	central_layout->addWidget(control_parent_widget);
	central_layout->addWidget(settings_button);

	control_parent_widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	central_layout->setAlignment(settings_button, Qt::AlignTop | Qt::AlignRight);
	settings_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	control_parent_widget->setLayout(control_parent_layout);
	control_parent_layout->addWidget(select_area_button);
	control_parent_layout->addWidget(toggle_popup_button);

	control_parent_layout->setAlignment(select_area_button, Qt::AlignCenter);
	select_area_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	control_parent_layout->setAlignment(toggle_popup_button, Qt::AlignCenter);
	toggle_popup_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	toggle_popup_button->setCheckable(true);
	toggle_popup_button->setChecked(true);
}
