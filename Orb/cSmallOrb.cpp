#include "stdafx.h"

#include "cSmallOrb.hpp"

cSmallOrb::cSmallOrb( QWidget* parent )
	: QWidget( parent )
{
	resize( 48, 48 );
	setAttribute( Qt::WA_TranslucentBackground );
}

void cSmallOrb::SetSvg( const QString& file )
{
	_svg.load( file );

	update();
}

void cSmallOrb::SetType( Type type )
{
    _type = type;
}

void cSmallOrb::SetState( State state )
{
    _state = state;
    update();
}

void cSmallOrb::paintEvent( QPaintEvent* paint_event )
{
    QPainter p( this );

    p.setRenderHint( QPainter::Antialiasing );

    QRectF orb = rect().adjusted( 2, 2, -2, -2 );

    //--------------------------
    // Base (뒤 배경이 비치지 않도록 불투명에 가까운 바탕)
    //--------------------------

    QColor base( 40, 42, 48, 200 );

    if( _type == Type::Toggle && _state == State::On )
        base = QColor( 0, 90, 65, 210 );

    if( _isHover == true )
        base.setAlpha( qMin( 255, base.alpha() + 30 ) );

    p.setBrush( base );
    p.setPen( Qt::NoPen );

    p.drawEllipse( orb );

    //--------------------------
    // Glass 하이라이트
    //--------------------------

    QColor bg( 255, 255, 255, 55 );

    if( _type == Type::Toggle && _state == State::On )
        bg = QColor( 0, 220, 160, 90 );

    if( _isHover == true )
        bg.setAlpha( bg.alpha() + 20 );

    p.setBrush( bg );
    p.setPen( Qt::NoPen );

    p.drawEllipse( orb );

    //--------------------------
    // Ambient Occlusion (가장자리 입체감)
    //--------------------------

    QRadialGradient ao( orb.center(), orb.width() / 2 );

    ao.setColorAt( 0.0, QColor( 0, 0, 0, 0 ) );
    ao.setColorAt( 0.7, QColor( 0, 0, 0, 0 ) );
    ao.setColorAt( 1.0, QColor( 10, 20, 30, 110 ) );

    p.setBrush( ao );
    p.setPen( Qt::NoPen );

    p.drawEllipse( orb );

    //--------------------------
    // 테두리
    //--------------------------

    QPen borderPen( QColor( 255, 255, 255, 90 ) );
    borderPen.setWidthF( 1.2 );

    p.setPen( borderPen );
    p.setBrush( Qt::NoBrush );

    p.drawEllipse( orb.adjusted( 0.5, 0.5, -0.5, -0.5 ) );

    //--------------------------
    // 아이콘
    //--------------------------

    QRectF icon = orb.adjusted( 10, 10, -10, -10 );

    _svg.render( &p, icon );
}

void cSmallOrb::mousePressEvent( QMouseEvent* mouse_event )
{
	emit Clicked();
	QWidget::mousePressEvent( mouse_event );
}

void cSmallOrb::enterEvent( QEnterEvent* enter_event )
{
    _isHover = true;
    update();
	QWidget::enterEvent( enter_event );
}

void cSmallOrb::leaveEvent( QEvent* event )
{
    _isHover = false;
    update();
	QWidget::leaveEvent( event );
}
