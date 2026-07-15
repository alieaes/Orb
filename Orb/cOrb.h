#pragma once

#include <QtWidgets/QMainWindow>

#include "cSettings.h"
#include "cSmallOrb.hpp"
#include "ui_cOrb.h"

class cStickyNote;

class cOrb : public QMainWindow
{
    Q_OBJECT

public:
    cOrb(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool );
    ~cOrb();

    void                   Snap( bool useCursorScreen = true );
    void                   OpenMenu();
    void                   CloseMenu();
    void                   CreateSmallOrbs();
    void                   ExpandSmallOrb();
	void                   CollapseSmallOrb();
    void                   ToggleMenu();

    void                   ToggleStickyNote();
    void                   ShowSettingsPopup( cSmallOrb* anchor );

    QPoint                 AdjustCenterToScreen( QPoint center, int radius );

    // 전체화면 오버레이 창을 쓰면 독점 전체화면 프로세스가 있을 때 화면이 깜빡이는 문제가 있어서,
    // 대신 전역 마우스 훅으로 "메뉴 바깥 클릭"을 감지한다. 훅 콜백(익명 네임스페이스 함수)에서 호출하므로 public.
    void                   OnGlobalMouseDown( const QPoint& screenPos );

private:
    void                   OpenStickyNotes();
    void                   CloseAllStickyNotes();

    cStickyNote*           CreateStickyNoteWindow();

public:
    bool                   eventFilter( QObject* watched, QEvent* event ) override;

protected:
    void                   paintEvent( QPaintEvent* event ) override;
    void                   mousePressEvent( QMouseEvent* event ) override;
    void                   mouseReleaseEvent( QMouseEvent* event ) override;
    void                   mouseMoveEvent( QMouseEvent* event ) override;
    void                   enterEvent( QEnterEvent* event ) override;
    void                   leaveEvent( QEvent* event ) override;
    void                   moveEvent( QMoveEvent* event ) override;

private:
    Ui::cOrbClass          ui;

    QPoint                 _dragOffset;
    QPoint                 _pressGlobalPos;

    bool                   _isPressed           = false;
    bool                   _isDragging          = false;

    QTimer                 _orbTimer;

    float                  _orbTime             = 0.0f;
    float                  _orbBlobSpeed        = 1.0f;

    float                  _orbScale            = 1.0f;

    QVariantAnimation*     _hoverAni            = nullptr;
    QVariantAnimation*     _clickAni            = nullptr;

    QVector<cSmallOrb*>    _vecMenu;
    bool                   _isMenuOpened        = false;

    bool                   _stickyNoteOn        = false;
    cSettings*             _settingsPopup       = nullptr;

    QVector<cStickyNote*>  _vecNotes;

    QTimer*                _positionSaveTimer   = nullptr;
    QTimer*                _topmostTimer        = nullptr;

    void*                  _mouseHook           = nullptr; // HHOOK - 헤더에 Windows.h를 안 끌어오려고 void*로 보관
};
