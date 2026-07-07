#pragma once

#include <QWidget>
#include <QtSvg/QtSvg>

class cSmallOrb : public QWidget
{
    Q_OBJECT
public:
    enum class Type
    {
        Toggle,
        Popup,
        Action
    };

    enum class State
    {
        Off,
        On
    };

    explicit      cSmallOrb( QWidget* parent = nullptr );

    void          SetSvg( const QString& file );
    void          SetType( Type type );
    void          SetState( State state );

signals:
    void          Clicked();

protected:
    void          paintEvent( QPaintEvent* ) override;
    void          mousePressEvent( QMouseEvent* ) override;
    void          enterEvent( QEnterEvent* ) override;
    void          leaveEvent( QEvent* ) override;

private:
    QSvgRenderer  _svg;
    Type          _type     = Type::Action;
    State         _state    = State::Off;

    bool          _isHover  = false;
};