#pragma once

#include <QtWidgets/QMainWindow>

#include "cOverlay.hpp"
#include "cSmallOrb.hpp"
#include "ui_cOrb.h"

class cOrb : public QMainWindow
{
    Q_OBJECT

public:
    cOrb(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool );
    ~cOrb();

    void   Snap();
    void   OpenMenu();
    void   CloseMenu();
    void   CreateSmallOrbs();
    void   ExpandSmallOrb();
	void   CollapseSmallOrb();
    void   ToggleMenu();

    void   ToggleStickyNote();
    void   ShowSettingsPopup( cSmallOrb* anchor );

    QPoint AdjustCenterToScreen( QPoint center, int radius );

public:
    bool   eventFilter( QObject* watched, QEvent* event ) override;

protected:
    void   paintEvent( QPaintEvent* event ) override;
    void   mousePressEvent( QMouseEvent* event ) override;
    void   mouseReleaseEvent( QMouseEvent* event ) override;
    void   mouseMoveEvent( QMouseEvent* event ) override;
    void   enterEvent( QEnterEvent* event ) override;
    void   leaveEvent( QEvent* event ) override;

private:
    Ui::cOrbClass      ui;

    QPoint             _dragOffset;
    QPoint             _pressGlobalPos;

    bool               _isPressed       = false;
    bool               _isDragging      = false;

    QTimer             _orbTimer;

    float              _orbTime         = 0.0f;
    float              _orbBlobSpeed    = 1.0f;

    float              _orbScale        = 1.0f;

    QVariantAnimation* _hoverAni        = nullptr;
    QVariantAnimation* _clickAni        = nullptr;

    QVector<cSmallOrb*> _vecMenu;
    bool                _isMenuOpened         = false;

    cOverlay*           _overlay        = nullptr;

    bool                _stickyNoteOn   = false;
    QWidget*            _settingsPopup  = nullptr;
};
