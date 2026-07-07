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

void cSmallOrb::paintEvent( QPaintEvent* paint_event )
{
    QPainter p( this );

    p.setRenderHint( QPainter::Antialiasing );

    QRectF orb = rect().adjusted( 2, 2, -2, -2 );

    //--------------------------
    // 배경
    //--------------------------

    QColor bg( 255, 255, 255, 40 );

    if( _isHover == true )
        bg.setAlpha( 70 );

    p.setBrush( bg );
    p.setPen( Qt::NoPen );

    p.drawEllipse( orb );

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
