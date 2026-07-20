#include "stdafx.h"
#include "cOrb.h"
#include <QtWidgets/QApplication>

#include <sodium.h>

int main( int argc, char* argv[] )
{
    if( sodium_init() < 0 )
        return -1;

    QApplication a( argc, argv );
    a.setQuitOnLastWindowClosed( false );

    cOrb w;
    w.show();
    return a.exec();
}
