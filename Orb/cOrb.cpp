#include "stdafx.h"
#include "cOrb.h"

#include <Windows.h>

#include <QInputDialog>
#include <QMenu>
#include <QSystemTrayIcon>

#include "Tools.hpp"
#include "cStickyNote.h"

namespace
{
    // 화면을 통째로 덮는 오버레이 창을 대신해서, 전역 저수준 마우스 훅으로 "메뉴 바깥 클릭"을 감지한다.
    // 오버레이 방식은 독점 전체화면 프로세스(게임 등)가 떠있을 때 작업표시줄이 사라졌다 나타나며
    // 화면이 깜빡이는 문제가 있어서 이 방식으로 대체했다.
    cOrb* g_orbInstance = nullptr;

    LRESULT CALLBACK MouseHookProc( int code, WPARAM wParam, LPARAM lParam )
    {
        if( code >= 0 && g_orbInstance != nullptr
            && ( wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ) )
        {
            auto* data = reinterpret_cast<MSLLHOOKSTRUCT*>( lParam );
            QPoint pos( data->pt.x, data->pt.y );
            cOrb* orb = g_orbInstance;

            QMetaObject::invokeMethod( orb, [ orb, pos ] ()
            {
                orb->OnGlobalMouseDown( pos );
            }, Qt::QueuedConnection );
        }

        return CallNextHookEx( nullptr, code, wParam, lParam );
    }
}

cOrb::cOrb( QWidget* parent, Qt::WindowFlags f )
	: QMainWindow( parent, f )
{
	ui.setupUi( this );
	setAttribute( Qt::WA_TranslucentBackground );
	resize( 96, 96 );

    setWindowIcon( QIcon( ":/cOrb/icons/icon.png" ) );

    Tools::CreateSettingFiles();

    // 꺼지기 전 위치로 복원. 저장된 좌표가 없거나 지금은 없는 모니터라면(모니터 구성이 바뀐 경우 등)
    // 화면 안으로 들어오게 보정한다.
    QString savedX = Tools::GetSettingValue( "Default", "OrbX", "" ).toString();

    if( savedX.isEmpty() == false )
    {
        int posX = savedX.toInt();
        int posY = Tools::GetSettingValue( "Default", "OrbY", 0 ).toInt();

        move( posX, posY );

        if( QGuiApplication::screenAt( frameGeometry().center() ) == nullptr )
        {
            QRect rc = QGuiApplication::primaryScreen()->availableGeometry();
            move( rc.right() - width() - 16, rc.bottom() - height() - 16 );
        }
    }

    _positionSaveTimer = new QTimer( this );
    _positionSaveTimer->setSingleShot( true );
    _positionSaveTimer->setInterval( 400 );

    connect( _positionSaveTimer, &QTimer::timeout, this, [ this ] ()
    {
        Tools::SetSettingValue( "Default", "OrbX", x() );
        Tools::SetSettingValue( "Default", "OrbY", y() );
    } );

    qApp->installEventFilter( this );

    connect( &_orbTimer, &QTimer::timeout, this, [ this ] ()
    {
        _orbTime += 0.03f * _orbBlobSpeed;
        update();
    } );

    _orbTimer.start( 16 );

    _hoverAni = new QVariantAnimation( this );
    _hoverAni->setDuration( 150 );
    _hoverAni->setEasingCurve( QEasingCurve::OutCubic );

    connect( _hoverAni, &QVariantAnimation::valueChanged, this, [ this ] ( const QVariant& v )
    {
        _orbScale = v.toFloat();
        update();
    } );

    CreateSmallOrbs();
    CreateTrayIcon();

    g_orbInstance = this;
    _mouseHook = ( void* )SetWindowsHookEx( WH_MOUSE_LL, MouseHookProc, GetModuleHandle( nullptr ), 0 );

    // 이전 세션에 스티키노트가 켜져 있었으면 복원한다. 단, 암호화가 걸려있으면
    // 비밀번호 없이 자동으로 켤 수 없으니 꺼진 채로 두고, 사용자가 직접 토글해서 풀어야 한다.
    bool wasStickyNoteOn = Tools::GetSettingValue( "Default", "StickyNoteOn", false ).toBool();
    bool isNoteEncrypted = Tools::GetSettingValue( "Default", "NoteEncrypt", false ).toBool();

    if( wasStickyNoteOn == true && isNoteEncrypted == false )
        OpenStickyNotes();

    // 시작할 때는 마우스 커서 위치가 아니라, 방금 복원한 오브 자신의 위치가 있는
    // 모니터를 기준으로 스냅해야 한다 (커서 기준으로 하면 실행 시점에 마우스가
    // 다른 모니터에 있을 때 엉뚱한 모니터로 끌려간다).
    Snap( false );

    // Qt의 WindowStaysOnTopHint는 전체화면 독점 게임 등이 뜨면 OS가 조용히 topmost를
    // 뺏어가도 알려주지 않는다. 주기적으로 다시 topmost로 올려서 원상복구한다.
    _topmostTimer = new QTimer( this );
    _topmostTimer->setInterval( 2000 );

    connect( _topmostTimer, &QTimer::timeout, this, [ this ] ()
    {
        SetWindowPos( ( HWND )winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
    } );

    _topmostTimer->start();
}

cOrb::~cOrb()
{
    if( _mouseHook != nullptr )
        UnhookWindowsHookEx( ( HHOOK )_mouseHook );

    if( g_orbInstance == this )
        g_orbInstance = nullptr;
}

void cOrb::OnGlobalMouseDown( const QPoint& screenPos )
{
    if( _isMenuOpened == false )
        return;

    if( frameGeometry().contains( screenPos ) == true )
        return;

    for( auto orb : _vecMenu )
    {
        if( orb->isVisible() == true && orb->frameGeometry().contains( screenPos ) == true )
            return;
    }

    CloseMenu();
}

void cOrb::Snap( bool useCursorScreen )
{
    QScreen* scr = useCursorScreen == true
        ? QGuiApplication::screenAt( QCursor::pos() )
        : QGuiApplication::screenAt( frameGeometry().center() );

    if( !scr )
        scr = QGuiApplication::primaryScreen();

    QRect screenRect = scr->availableGeometry();

    QPoint pos = this->pos();

    int left = pos.x() - screenRect.left();
    int right = screenRect.right() - ( pos.x() + width() );

    int top = pos.y() - screenRect.top();
    int bottom = screenRect.bottom() - ( pos.y() + height() );

    int d = qMin( qMin( left, right ), qMin( top, bottom ) );

    QPoint target = pos;
    constexpr int margin = 16;

    if( d == left )
        target.setX( screenRect.left() + margin );
    else if( d == right )
        target.setX( screenRect.right() - width() - margin );
    else if( d == top )
        target.setY( screenRect.top() + margin );
    else
        target.setY( screenRect.bottom() - height() - margin );

    QPropertyAnimation* ani = new QPropertyAnimation( this, "pos" );

    ani->setDuration( 180 );
    ani->setStartValue( pos );
    ani->setEndValue( target );
    ani->setEasingCurve( QEasingCurve::OutCubic );

    ani->start( QAbstractAnimation::DeleteWhenStopped );
}

void cOrb::OpenMenu()
{
    if( _isMenuOpened == true )
        return;

    _isMenuOpened = true;

    raise();
    activateWindow();

    ExpandSmallOrb();
}

void cOrb::CloseMenu()
{
    if( _isMenuOpened == false )
        return;

    _isMenuOpened = false;

    CollapseSmallOrb();
    Snap();
}

void cOrb::CreateSmallOrbs()
{
    for( int idx = 0; idx < 6; idx++ )
    {
        auto orb = new cSmallOrb( nullptr );

        orb->setWindowFlags( Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint );
        orb->resize( 48, 48 );

        orb->hide();

        _vecMenu.push_back( orb );
    }

    // 12시 : 스티키 노트 on/off 토글
    auto stickyOrb = _vecMenu[ 0 ];

    stickyOrb->SetSvg( ":/cOrb/icons/notepad.svg" );
    stickyOrb->SetType( cSmallOrb::Type::Toggle );

    connect( stickyOrb, &cSmallOrb::Clicked, this, &cOrb::ToggleStickyNote );

    // 6시 : 설정 팝업
    auto settingsOrb = _vecMenu[ 3 ];

    settingsOrb->SetSvg( ":/cOrb/icons/settings.svg" );
    settingsOrb->SetType( cSmallOrb::Type::Popup );

    connect( settingsOrb, &cSmallOrb::Clicked, this, [ this, settingsOrb ] ()
    {
        ShowSettingsPopup( settingsOrb );
    } );
}

void cOrb::ToggleStickyNote()
{
    bool turningOn = ( _stickyNoteOn == false );

    if( turningOn == false )
    {
        CloseAllStickyNotes();
        return;
    }

    if( Tools::GetSettingValue( "Default", "NoteEncrypt", false ).toBool() == true
        && Tools::HasNoteKey() == false )
    {
        bool ok = false;

        QString password = QInputDialog::getText( this, "노트 잠금 해제",
            "노트를 열려면 비밀번호를 입력하세요.", QLineEdit::Password, "", &ok );

        if( ok == false || password.isEmpty() == true )
            return; // 취소 - 토글 안 함

        if( Tools::UnlockNoteKey( password ) == false )
        {
            QMessageBox::warning( this, "노트 잠금 해제", "비밀번호가 올바르지 않습니다." );
            return;
        }
    }

    OpenStickyNotes();
}

void cOrb::OpenStickyNotes()
{
    QStringList guids = Tools::GetSettingValue( "Default", "OpenNotes", "" )
        .toString().split( ",", Qt::SkipEmptyParts );

    if( guids.isEmpty() == true )
    {
        auto note = CreateStickyNoteWindow();
        note->CreateNewNote();
        note->show();
    }
    else
    {
        for( const QString& guid : guids )
        {
            auto note = CreateStickyNoteWindow();
            note->LoadNote( guid );
            note->show();
        }
    }

    _stickyNoteOn = true;
    _vecMenu[ 0 ]->SetState( cSmallOrb::State::On );
    Tools::SetSettingValue( "Default", "StickyNoteOn", true );
}

void cOrb::CloseAllStickyNotes()
{
    // 개별 노트가 자기 X 버튼으로 닫힐 때와 달리, 마스터 토글로 끌 때는
    // "열려있던 목록"에서 지우지 않는다 - 다음에 다시 켜면 그대로 복원하기 위함.
    for( auto note : _vecNotes )
    {
        note->CloseForSession();
        note->deleteLater();
    }

    _vecNotes.clear();

    _stickyNoteOn = false;
    _vecMenu[ 0 ]->SetState( cSmallOrb::State::Off );
    Tools::SetSettingValue( "Default", "StickyNoteOn", false );
}

cStickyNote* cOrb::CreateStickyNoteWindow()
{
    auto note = new cStickyNote( nullptr );

    connect( note, &cStickyNote::Closed, this, [ this ] ( cStickyNote* self )
    {
        _vecNotes.removeAll( self );
        self->deleteLater();

        if( _vecNotes.isEmpty() == true )
        {
            _stickyNoteOn = false;
            _vecMenu[ 0 ]->SetState( cSmallOrb::State::Off );
            Tools::SetSettingValue( "Default", "StickyNoteOn", false );
        }
    } );

    connect( note, &cStickyNote::RequestNewNote, this, [ this ] ()
    {
        auto newNote = CreateStickyNoteWindow();
        newNote->CreateNewNote();
        newNote->show();
    } );

    _vecNotes.push_back( note );

    return note;
}

void cOrb::ShowSettingsPopup( cSmallOrb* anchor )
{
    if( _settingsPopup != nullptr )
    {
        _settingsPopup->close();
        _settingsPopup = nullptr;
        return;
    }

    auto settings = new cSettings( nullptr );

    settings->setWindowFlags( Qt::Tool | Qt::FramelessWindowHint );
    settings->setAttribute( Qt::WA_DeleteOnClose );

    // 오브가 있는 화면의 정 중앙에 띄운다 (오브가 화면 아래쪽에 있으면 anchor 기준으론 화면 밖으로 나가버림)
    QScreen* screen = QGuiApplication::screenAt( frameGeometry().center() );
    if( !screen )
        screen = QGuiApplication::primaryScreen();

    QRect rc = screen->availableGeometry();

    settings->move( rc.center().x() - settings->width() / 2,
                     rc.center().y() - settings->height() / 2 );

    _settingsPopup = settings;

    connect( settings, &QObject::destroyed, this, [ this ] ()
    {
        _settingsPopup = nullptr;
    } );

    settings->show();
    settings->raise();
    settings->activateWindow();
}

void cOrb::CreateTrayIcon()
{
    _trayMenu = new QMenu();

    QAction* toggleAction = _trayMenu->addAction( "오브 보이기/숨기기" );
    connect( toggleAction, &QAction::triggered, this, &cOrb::ToggleOrbVisible );

    _trayMenu->addSeparator();

    QAction* exitAction = _trayMenu->addAction( "종료" );
    connect( exitAction, &QAction::triggered, qApp, &QApplication::quit );

    _trayIcon = new QSystemTrayIcon( QIcon( ":/cOrb/icons/icon.png" ), this );
    _trayIcon->setToolTip( "Orb" );
    _trayIcon->setContextMenu( _trayMenu );

    connect( _trayIcon, &QSystemTrayIcon::activated, this, [ this ] ( QSystemTrayIcon::ActivationReason reason )
    {
        if( reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick )
            ToggleOrbVisible();
    } );

    _trayIcon->show();
}

void cOrb::ToggleOrbVisible()
{
    if( isVisible() == true )
    {
        hide();
    }
    else
    {
        show();
        raise();
        activateWindow();
    }
}

void cOrb::ExpandSmallOrb()
{
    constexpr int radius = 75;

    QPoint center = mapToGlobal( rect().center() );
    center = AdjustCenterToScreen( center, radius );

    QScreen* screen = QGuiApplication::screenAt( center );
    if( !screen )
        screen = QGuiApplication::primaryScreen();

    QRect rc = screen->availableGeometry();

    //---------------------------------------
    // 먼저 목표 위치 계산
    //---------------------------------------

    QVector<QPoint> targets;

    for( int idx = 0; idx < _vecMenu.size(); idx++ )
    {
        auto orb = _vecMenu[ idx ];

        double angle = -90 + idx * 60;
        double rad = qDegreesToRadians( angle );

        QPoint target( center.x() + std::cos( rad ) * radius - orb->width() / 2, center.y() + std::sin( rad ) * radius - orb->height() / 2 );
        targets.push_back( target );
    }

    //---------------------------------------
    // 화면 밖으로 나가는지 검사
    //---------------------------------------

    int offsetX = 0;
    int offsetY = 0;

    for( int idx = 0; idx < targets.size(); idx++ )
    {
        auto orb = _vecMenu[ idx ];

        QRect r( targets[ idx ], orb->size() );

        if( r.left() < rc.left() )
            offsetX = qMax( offsetX, rc.left() - r.left() );

        if( r.right() > rc.right() )
            offsetX = qMin( offsetX, rc.right() - r.right() );

        if( r.top() < rc.top() )
            offsetY = qMax( offsetY, rc.top() - r.top() );

        if( r.bottom() > rc.bottom() )
            offsetY = qMin( offsetY, rc.bottom() - r.bottom() );
    }

    //---------------------------------------
    // 애니메이션
    //---------------------------------------

    QPoint start( center.x() - 24, center.y() - 24 );

    for( int idx = 0; idx < _vecMenu.size(); idx++ )
    {
        auto orb = _vecMenu[ idx ];

        QPoint target = targets[ idx ] + QPoint( offsetX, offsetY );

        orb->move( start );

        orb->show();
        orb->raise();

        auto ani = new QPropertyAnimation( orb, "pos" );

        ani->setDuration( 250 );
        ani->setStartValue( start );
        ani->setEndValue( target );
        ani->setEasingCurve( QEasingCurve::OutBack );

        ani->start( QAbstractAnimation::DeleteWhenStopped );
    }
}

void cOrb::CollapseSmallOrb()
{
    QPoint center = mapToGlobal( rect().center() );

    for( auto orb : _vecMenu )
    {
        QPoint target(
            center.x() - orb->width() / 2,
            center.y() - orb->height() / 2 );

        auto ani =
            new QPropertyAnimation( orb, "pos" );

        ani->setDuration( 180 );

        ani->setStartValue( orb->pos() );

        ani->setEndValue( target );

        ani->setEasingCurve( QEasingCurve::InBack );

        connect(
            ani,
            &QPropertyAnimation::finished,
            orb,
            &QWidget::hide );

        ani->start(
            QAbstractAnimation::DeleteWhenStopped );
    }
}

void cOrb::ToggleMenu()
{
    if( _isMenuOpened == true )
        CloseMenu();
    else
        OpenMenu();
}

QPoint cOrb::AdjustCenterToScreen( QPoint center, int radius )
{
    constexpr int orbSize = 48;

    QScreen* screen = QGuiApplication::screenAt( center );
    if( !screen )
        screen = QGuiApplication::primaryScreen();

    QRect rc = screen->availableGeometry();

    int offsetX = 0;
    int offsetY = 0;

    for( int idx = 0; idx < _vecMenu.size(); ++idx )
    {
        double angle = -90 + idx * 60;
        double rad = qDegreesToRadians( angle );

        QPoint p(
            center.x() + std::cos( rad ) * radius - orbSize / 2,
            center.y() + std::sin( rad ) * radius - orbSize / 2 );

        QRect r( p, QSize( orbSize, orbSize ) );

        if( r.left() < rc.left() )
            offsetX = qMax( offsetX, rc.left() - r.left() );

        if( r.right() > rc.right() )
            offsetX = qMin( offsetX, rc.right() - r.right() );

        if( r.top() < rc.top() )
            offsetY = qMax( offsetY, rc.top() - r.top() );

        if( r.bottom() > rc.bottom() )
            offsetY = qMin( offsetY, rc.bottom() - r.bottom() );
    }

    if( offsetX != 0 || offsetY != 0 )
    {
        move( pos() + QPoint( offsetX, offsetY ) );

        // move() 후에는 center가 바뀌므로 다시 계산해서 반환
        center = mapToGlobal( rect().center() );
    }

    return center;
}

bool cOrb::eventFilter( QObject* watched, QEvent* event )
{
    return QMainWindow::eventFilter( watched, event );
}

void cOrb::paintEvent( QPaintEvent* event )
{
    QPainter p( this );

    p.setRenderHint( QPainter::Antialiasing );
    p.setRenderHint( QPainter::SmoothPixmapTransform );

    QPointF center = rect().center();

    p.translate( center );
    p.scale( _orbScale, _orbScale );
    p.translate( -center );

    QRectF orb( 8, 8, 80, 80 );

    //-----------------------------------------
    // Clip
    //-----------------------------------------

    QPainterPath clip;
    clip.addEllipse( orb );

    p.setClipPath( clip );

    //-----------------------------------------
    // Ambient Occlusion (배경이 밝아도 윤곽이 살도록)
    //-----------------------------------------

    QRadialGradient ao( orb.center(), orb.width() / 2 );

    ao.setColorAt( 0.0, QColor( 0, 0, 0, 0 ) );
    ao.setColorAt( 0.78, QColor( 0, 0, 0, 0 ) );
    ao.setColorAt( 1.0, QColor( 10, 20, 30, 90 ) );

    p.setBrush( ao );
    p.setPen( Qt::NoPen );
    p.drawEllipse( orb );

    //-----------------------------------------
    // Glass Background
    //-----------------------------------------

    // p.fillRect( rect(), QColor( 35, 35, 40, 40 ) );
    p.fillRect( rect(), QColor( 255, 255, 255, 35 ) );

    //-----------------------------------------
    // Mesh Gradient
    //-----------------------------------------

    auto drawBlob =
        [ & ] ( QColor color, QPointF center, float radius )
    {
        QRadialGradient g( center, radius );

        color.setAlpha( 160 );

        g.setColorAt( 0, color );

        color.setAlpha( 30 );

        g.setColorAt( 1, color );

        p.setBrush( g );
        p.setPen( Qt::NoPen );

        p.drawEllipse( center, radius, radius );
    };

    drawBlob(
        QColor( 0, 170, 255 ),
        QPointF( width() / 2 + sin( _orbTime * 0.9 ) * 22,
                 height() / 2 + cos( _orbTime * 1.1 ) * 16 ),
        34 );

    drawBlob(
        QColor( 180, 80, 255 ),
        QPointF( width() / 2 + cos( _orbTime * 1.3 + 1.2 ) * 18,
                 height() / 2 + sin( _orbTime * 0.8 ) * 24 ),
        30 );

    drawBlob(
        QColor( 0, 255, 180 ),
        QPointF( width() / 2 + sin( _orbTime * 1.7 + 2.1 ) * 16,
                 height() / 2 + cos( _orbTime * 1.5 + 0.5 ) * 18 ),
        26 );

    drawBlob(
        QColor( 255, 180, 80 ),
        QPointF( width() / 2 + cos( _orbTime * 1.1 + 3.14 ) * 20,
                 height() / 2 + sin( _orbTime * 1.2 + 1.7 ) * 20 ),
        28 );

    //-----------------------------------------
    // Liquid
    //-----------------------------------------
    //
    //QPainterPath wave;
    //
    //wave.moveTo( 0, height() );
    //
    //for( int x = 0; x <= width(); x++ )
    //{
    //    float y =
    //        height() / 2
    //        + sin( x * 0.09 + _orbTime * 3.0 ) * 4
    //        + cos( x * 0.04 + _orbTime * 1.7 ) * 2;
    //
    //    wave.lineTo( x, y );
    //}
    //
    //wave.lineTo( width(), height() );
    //wave.closeSubpath();
    //
    //p.fillPath(
    //    wave,
    //    QColor( 255, 255, 255, 28 ) );

    //-----------------------------------------
    // Core Glow
    //-----------------------------------------

    float pulse =
        1.0f +
        sin( _orbTime * 2.0 ) * 0.08f;

    float radius =
        width() * 0.14f * pulse;

    QRadialGradient core(
        orb.center(),
        radius * 2 );

    core.setColorAt(
        0,
        QColor( 220, 245, 255, 220 ) );

    core.setColorAt(
        0.4,
        QColor( 80, 200, 255, 180 ) );

    core.setColorAt(
        1,
        QColor( 120, 220, 255, 0 ) );

    p.setBrush( core );

    p.drawEllipse(
        orb.center(),
        radius * 2,
        radius * 2 );

    //-----------------------------------------
    // Highlight
    //-----------------------------------------

    QLinearGradient shine(
        orb.topLeft(),
        orb.bottomLeft() );

    shine.setColorAt(
        0,
        QColor( 255, 255, 255, 120 ) );

    shine.setColorAt(
        0.5,
        QColor( 255, 255, 255, 40 ) );

    shine.setColorAt(
        1,
        QColor( 255, 255, 255, 0 ) );

    p.setBrush( shine );

    p.drawEllipse( orb );

    //-----------------------------------------
    // Border
    //-----------------------------------------

    //QRadialGradient edgeGlow( orb.center(), orb.width() / 2 );
    //
    //edgeGlow.setColorAt( 0.75, QColor( 255, 255, 255, 0 ) );
    //edgeGlow.setColorAt( 1.00, QColor( 255, 255, 255, 50 ) );

    QRadialGradient edgeGlow( orb.center(), orb.width() / 2 );

    edgeGlow.setColorAt( 0.75, QColor( 100, 200, 255, 0 ) );
    edgeGlow.setColorAt( 0.94, QColor( 160, 220, 255, 55 ) );
    edgeGlow.setColorAt( 1.00, QColor( 30, 50, 70, 90 ) );

    p.setBrush( edgeGlow );
    p.setPen( Qt::NoPen );
    p.drawEllipse( orb );
}

void cOrb::mousePressEvent( QMouseEvent* event )
{
    if( event->button() != Qt::LeftButton )
        return;

    _isPressed = true;
    _isDragging = false;

    _pressGlobalPos = event->globalPosition().toPoint();
    _dragOffset = _pressGlobalPos - frameGeometry().topLeft();
}

void cOrb::mouseReleaseEvent( QMouseEvent* event )
{
    if( event->button() != Qt::LeftButton )
        return;

    if( _isDragging == false )
    {
        // 움직이지 않았으면 클릭
        ToggleMenu();
    }
    else
    {
        // 드래그였다면 스냅
        Snap();
    }

    _isPressed = false;
    _isDragging = false;
}

void cOrb::mouseMoveEvent( QMouseEvent* event )
{
    if( _isPressed == false )
        return;

    QPoint globalPos = event->globalPosition().toPoint();

    // 5픽셀 이상 움직이면 드래그 시작
    if( _isDragging == false && ( globalPos - _pressGlobalPos ).manhattanLength() > 5 )
    {
        _isDragging = true;

        // 드래그 중 메뉴가 열려 있으면 닫기
        if( _isMenuOpened == true )
            CloseMenu();
    }

    if( _isDragging == true )
    {
        move( globalPos - _dragOffset );

        if( _isMenuOpened == true )
            ExpandSmallOrb();   // 위치만 갱신하는 용도
    }
}

void cOrb::enterEvent( QEnterEvent* event )
{
    _orbBlobSpeed = 2.0f;

    // Hover Scale
    _hoverAni->stop();
    _hoverAni->setStartValue( _orbScale );
    _hoverAni->setEndValue( 1.08f );
    _hoverAni->start();

	QMainWindow::enterEvent( event );
}

void cOrb::leaveEvent( QEvent* event )
{
    _orbBlobSpeed = 1.0f;

    _hoverAni->stop();
    _hoverAni->setStartValue( _orbScale );
    _hoverAni->setEndValue( 1.0f );
    _hoverAni->start();

	QMainWindow::leaveEvent( event );
}

void cOrb::moveEvent( QMoveEvent* event )
{
    if( _positionSaveTimer != nullptr )
        _positionSaveTimer->start();

    QMainWindow::moveEvent( event );
}
