#include "stdafx.h"
#include "cOverlay.hpp"

cOverlay::cOverlay( QWidget* parent )
    : QWidget( parent )
{
    setWindowFlags( Qt::FramelessWindowHint | Qt::Tool );

    setAttribute( Qt::WA_TranslucentBackground );
    setAttribute( Qt::WA_NoSystemBackground );
    setAttribute( Qt::WA_TransparentForMouseEvents, false );

    setStyleSheet( "" );
    setFocusPolicy( Qt::NoFocus );
    hide();
}

void cOverlay::mousePressEvent( QMouseEvent* )
{
    emit ClickedOutside();
}