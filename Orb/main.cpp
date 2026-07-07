#include "stdafx.h"
#include "cOrb.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    cOrb w;
    w.show();
    return a.exec();
}
