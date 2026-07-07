#include "stdafx.h"
#include "cOrb.h"

#include <QGraphicsDropShadowEffect>

#include <Windows.h>

cOrb::cOrb( QWidget* parent, Qt::WindowFlags f )
	: QMainWindow( parent, f )
{
	ui.setupUi( this );
	setAttribute( Qt::WA_TranslucentBackground );
	resize( 96, 96 );

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

    // 밝은 배경 위에서도 윤곽이 살도록 배경과 분리되는 그림자를 항상 붙여준다
    auto shadow = new QGraphicsDropShadowEffect( this );
    shadow->setBlurRadius( 24 );
    shadow->setOffset( 0, 6 );
    shadow->setColor( QColor( 0, 0, 0, 110 ) );
    setGraphicsEffect( shadow );

    _overlay = new cOverlay;
    connect( _overlay, &cOverlay::ClickedOutside, this, &cOrb::CloseMenu );
}

cOrb::~cOrb()
{}

void cOrb::Snap()
{
    QScreen* scr = QGuiApplication::screenAt( QCursor::pos() );
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

    QScreen* screen = QGuiApplication::screenAt( frameGeometry().center() );

    if( screen == nullptr )
        screen = QGuiApplication::primaryScreen();

    _overlay->setGeometry( screen->geometry() );

    _overlay->show();

    HWND hwnd = ( HWND )_overlay->winId();

    LONG ex = GetWindowLong( hwnd, GWL_EXSTYLE );

    SetWindowLong( hwnd,
                   GWL_EXSTYLE,
                   ex | WS_EX_NOACTIVATE );

    // Overlay보다 위로
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

    _overlay->hide();
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

        QPoint target(
            center.x() + std::cos( rad ) * radius - orb->width() / 2,
            center.y() + std::sin( rad ) * radius - orb->height() / 2 );

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

    QPoint start(
        center.x() - 24,
        center.y() - 24 );

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
