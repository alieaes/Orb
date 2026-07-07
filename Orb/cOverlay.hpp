#pragma once

#include <QWidget>

class cOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit cOverlay( QWidget* parent = nullptr );

signals:
    void ClickedOutside();

protected:
    void mousePressEvent( QMouseEvent* e ) override;
};