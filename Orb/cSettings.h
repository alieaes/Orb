#pragma once

#include <QWidget>
#include "ui_cSettings.h"

#include "QT/ExQtSwitch.hpp"

class cSettings : public QWidget
{
	Q_OBJECT

public:
	cSettings( QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool );
	~cSettings();

	bool NoteDecrypt();
	bool NoteEncrypt();

private:
	Ui::cSettingsClass ui;
};
