#include "stdafx.h"
#include "cStickyNote.h"

#include "Tools.hpp"

#include <QMenu>
#include <QSaveFile>
#include <QStandardItemModel>
#include <QUuid>

#include <Windows.h>
#include <windowsx.h>

QSet<QString> cStickyNote::s_openGuids;

cStickyNote::cStickyNote( QWidget* parent )
	: QWidget( parent )
{
	ui.setupUi( this );

	setWindowFlags( Qt::FramelessWindowHint | Qt::Tool );

	// frame/frame_2/frame_3/frame_4/frMenu가 전부 StyledPanel+Raised라 중첩된 패널마다
	// 각자 입체(엠보싱) 테두리가 겹쳐 그려져서 "튀어나온" 것처럼 보인다.
	// 바깥 테두리(frame) 하나만 얇게 남기고 안쪽은 다 평평하게 정리한다.
	ui.frame_2->setFrameShape( QFrame::NoFrame );
	ui.frame_3->setFrameShape( QFrame::NoFrame );
	ui.frame_4->setFrameShape( QFrame::NoFrame );
	ui.frMenu->setFrameShape( QFrame::NoFrame );

	setStyleSheet(
	    "#frame { border: 1px solid rgba(255, 255, 255, 35); }"
	    "#frame_2 { background-color: rgba(255, 255, 255, 18); border-bottom: 1px solid rgba(255, 255, 255, 35); }" );

	// 타이틀바 좌측 여백 (버튼들처럼 딱 붙어있으면 답답해 보인다)
	ui.lblTitle->setContentsMargins( 8, 0, 0, 0 );

	ui.btnAdd->setIcon( QIcon( ":/cOrb/icons/plus.svg" ) );
	ui.btnAdd->setIconSize( QSize( 14, 14 ) );
	ui.btnAdd->setCursor( Qt::PointingHandCursor );

	ui.btnList->setIcon( QIcon( ":/cOrb/icons/list.svg" ) );
	ui.btnList->setIconSize( QSize( 14, 14 ) );
	ui.btnList->setCursor( Qt::PointingHandCursor );

	ui.btnPinned->setIcon( QIcon( ":/cOrb/icons/clip.svg" ) );
	ui.btnPinned->setIconSize( QSize( 14, 14 ) );
	ui.btnPinned->setCursor( Qt::PointingHandCursor );
	ui.btnPinned->setCheckable( true );
	ui.btnPinned->setStyleSheet(
	    "QPushButton { border: 1px solid transparent; border-radius: 4px; background: transparent; }"
	    "QPushButton:hover { background: rgba(255, 255, 255, 25); }"
	    "QPushButton:checked { border: 1px solid rgba(0, 200, 140, 200); background: rgba(0, 200, 140, 60); }" );

	ui.btnClose->setIcon( QIcon( ":/cOrb/icons/close.svg" ) );
	ui.btnClose->setIconSize( QSize( 14, 14 ) );
	ui.btnClose->setCursor( Qt::PointingHandCursor );

	// 0까지 내리면 창이 완전히 안 보여서 다시 찾을 수 없게 되니 최소치를 둔다
	ui.SldTrans->setMinimum( 10 );

	// 서식(볼드/밑줄/폰트크기)은 일단 보류 - 지금은 순수 텍스트만
	ui.edtText->setAcceptRichText( false );

	// .ui의 frame_3에 16pt가 박혀있어서 그대로 상속되면 너무 크다 - 명시적으로 재설정
	QFont textFont = ui.edtText->font();
	textFont.setPointSize( 10 );
	ui.edtText->setFont( textFont );

	_saveTimer = new QTimer( this );
	_saveTimer->setSingleShot( true );
	_saveTimer->setInterval( 500 );

	connect( _saveTimer, &QTimer::timeout, this, [ this ] ()
	{
		SaveContent();
		SaveMeta();
	} );

	_topmostTimer = new QTimer( this );
	_topmostTimer->setInterval( 2000 );

	connect( _topmostTimer, &QTimer::timeout, this, [ this ] ()
	{
		// 전체화면 게임 등이 떠서 OS가 topmost를 조용히 뺏어갔을 수 있으니, 고정된 노트만 주기적으로 다시 올려준다.
		if( _isPinned == true )
			SetWindowPos( ( HWND )winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	} );

	_topmostTimer->start();

	ui.listView->setContextMenuPolicy( Qt::CustomContextMenu );

	connect( ui.listView, &QListView::customContextMenuRequested, this, [ this ] ( const QPoint& pos )
	{
		QModelIndex index = ui.listView->indexAt( pos );

		if( index.isValid() == false )
			return;

		QString guid = index.data( Qt::UserRole ).toString();

		if( guid.isEmpty() == true )
			return;

		QMenu menu( this );
		QAction* deleteAction = menu.addAction( "노트 삭제" );

		if( menu.exec( ui.listView->viewport()->mapToGlobal( pos ) ) == deleteAction )
			RequestDeleteNote( guid );
	} );

	connect( ui.btnAdd, &QPushButton::clicked, this, &cStickyNote::RequestNewNote );

	connect( ui.btnClose, &QPushButton::clicked, this, [ this ] ()
	{
		_explicitClose = true;
		close();
	} );

	connect( ui.btnPinned, &QPushButton::toggled, this, [ this ] ( bool checked )
	{
		ApplyPinned( checked );
		_saveTimer->start();
	} );

	connect( ui.btnList, &QPushButton::clicked, this, [ this ] ()
	{
		if( ui.stackedMain->currentWidget() == ui.wdgList )
		{
			ui.stackedMain->setCurrentWidget( ui.wdgMemo );
		}
		else
		{
			RefreshListView();
			ui.stackedMain->setCurrentWidget( ui.wdgList );
		}
	} );

	connect( ui.SldTrans, &QSlider::valueChanged, this, [ this ] ( int value )
	{
		setWindowOpacity( value / 100.0 );
		_saveTimer->start();
	} );

	connect( ui.edtText, &QTextEdit::textChanged, this, [ this ] ()
	{
		UpdateTitleFromContent();
		_saveTimer->start();
	} );

	connect( ui.listView, &QListView::clicked, this, [ this ] ( const QModelIndex& index )
	{
		QString guid = index.data( Qt::UserRole ).toString();

		if( guid.isEmpty() == true || IsGuidOpen( guid ) == true )
			return;

		SwitchToNote( guid );
		ui.stackedMain->setCurrentWidget( ui.wdgMemo );
	} );
}

cStickyNote::~cStickyNote()
{}

bool cStickyNote::IsGuidOpen( const QString& guid )
{
	return s_openGuids.contains( guid );
}

void cStickyNote::PersistOpenGuids()
{
	Tools::SetSettingValue( "Default", "OpenNotes", QStringList( s_openGuids.values() ).join( "," ) );
}

QString cStickyNote::NoteFilePath( const QString& guid ) const
{
	return Tools::GetSettingValue( "Default", "NoteSavePath", "" ).toString() + "/" + guid + ".txt";
}

QString cStickyNote::SettingGroup( const QString& guid ) const
{
	return "Note_" + guid;
}

void cStickyNote::LoadNote( const QString& guid )
{
	_guid = guid;

	QString content;

	QFile file( NoteFilePath( guid ) );

	if( file.open( QIODevice::ReadOnly ) == true )
	{
		QByteArray data = file.readAll();
		file.close();

		bool isEncrypt = Tools::GetSettingValue( "Default", "NoteEncrypt", false ).toBool();

		if( isEncrypt == true )
		{
			bool ok = false;
			content = Tools::DecryptNote( data, &ok );

			if( ok == false )
				QMessageBox::warning( this, "노트", "노트 내용을 복호화하지 못했습니다." );
		}
		else
		{
			content = QString::fromUtf8( data );
		}
	}

	ui.edtText->blockSignals( true );
	ui.edtText->setPlainText( content );
	ui.edtText->blockSignals( false );

	QString group = SettingGroup( guid );

	QString title = Tools::GetSettingValue( group, "Title", "" ).toString();

	int x = Tools::GetSettingValue( group, "X", geometry().x() ).toInt();
	int y = Tools::GetSettingValue( group, "Y", geometry().y() ).toInt();
	int w = Tools::GetSettingValue( group, "W", 320 ).toInt();
	int h = Tools::GetSettingValue( group, "H", 240 ).toInt();
	bool pinned = Tools::GetSettingValue( group, "Pinned", false ).toBool();
	int opacity = qMax( 10, Tools::GetSettingValue( group, "Opacity", 100 ).toInt() ); // 예전에 저장된 값이 10 미만일 수도 있으니 방어

	resize( w, h );
	move( x, y );

	ui.SldTrans->blockSignals( true );
	ui.SldTrans->setValue( opacity );
	ui.SldTrans->blockSignals( false );
	setWindowOpacity( opacity / 100.0 );

	ui.btnPinned->blockSignals( true );
	ui.btnPinned->setChecked( pinned );
	ui.btnPinned->blockSignals( false );
	ApplyPinned( pinned );

	if( title.isEmpty() == true )
		UpdateTitleFromContent();
	else
		ui.lblTitle->setText( title );

	ui.stackedMain->setCurrentWidget( ui.wdgMemo );

	s_openGuids.insert( guid );
	PersistOpenGuids();
}

void cStickyNote::CreateNewNote()
{
	_guid = QUuid::createUuid().toString( QUuid::WithoutBraces );

	ui.edtText->blockSignals( true );
	ui.edtText->clear();
	ui.edtText->blockSignals( false );

	ui.lblTitle->setText( "(빈 노트)" );

	// 겹치지 않게 이미 열려있는 노트 수만큼 살짝 대각선으로 어긋나게 배치
	QPoint base( 120, 120 );
	QPoint offset( ( s_openGuids.size() % 10 ) * 24, ( s_openGuids.size() % 10 ) * 24 );

	resize( 320, 240 );
	move( base + offset );

	ui.SldTrans->blockSignals( true );
	ui.SldTrans->setValue( 100 );
	ui.SldTrans->blockSignals( false );
	setWindowOpacity( 1.0 );

	ui.btnPinned->blockSignals( true );
	ui.btnPinned->setChecked( false );
	ui.btnPinned->blockSignals( false );
	ApplyPinned( false );

	ui.stackedMain->setCurrentWidget( ui.wdgMemo );

	s_openGuids.insert( _guid );
	PersistOpenGuids();

	SaveContent();
	SaveMeta();
}

void cStickyNote::SwitchToNote( const QString& guid )
{
	SaveContent();
	SaveMeta();

	s_openGuids.remove( _guid );

	LoadNote( guid ); // LoadNote가 새 guid를 다시 s_openGuids에 넣고 PersistOpenGuids도 호출함
}

void cStickyNote::SaveContent()
{
	if( _guid.isEmpty() == true )
		return;

	QString text = ui.edtText->toPlainText();

	bool isEncrypt = Tools::GetSettingValue( "Default", "NoteEncrypt", false ).toBool();

	QByteArray data;

	if( isEncrypt == true )
	{
		if( Tools::HasNoteKey() == false )
			return; // 세션에 키가 없으면 저장하지 않는다 (평문/깨진 데이터로 덮어쓰는 것 방지)

		data = Tools::EncryptNote( text );
	}
	else
	{
		data = text.toUtf8();
	}

	QSaveFile saveFile( NoteFilePath( _guid ) );

	if( saveFile.open( QIODevice::WriteOnly ) == false )
		return;

	saveFile.write( data );
	saveFile.commit();
}

void cStickyNote::SaveMeta()
{
	if( _guid.isEmpty() == true )
		return;

	QString group = SettingGroup( _guid );

	Tools::SetSettingValue( group, "Title", ui.lblTitle->text() );
	Tools::SetSettingValue( group, "X", x() );
	Tools::SetSettingValue( group, "Y", y() );
	Tools::SetSettingValue( group, "W", width() );
	Tools::SetSettingValue( group, "H", height() );
	Tools::SetSettingValue( group, "Pinned", _isPinned );
	Tools::SetSettingValue( group, "Opacity", ui.SldTrans->value() );
}

void cStickyNote::UpdateTitleFromContent()
{
	QString firstLine = ui.edtText->toPlainText().section( '\n', 0, 0 ).trimmed();

	ui.lblTitle->setText( firstLine.isEmpty() == true ? "(빈 노트)" : firstLine.left( 30 ) );
}

void cStickyNote::ApplyPinned( bool pinned )
{
	_isPinned = pinned;

	Qt::WindowFlags flags = windowFlags();

	if( pinned == true )
		flags |= Qt::WindowStaysOnTopHint;
	else
		flags &= ~Qt::WindowStaysOnTopHint;

	bool wasVisible = isVisible();

	setWindowFlags( flags );

	if( wasVisible == true )
		show(); // 윈도우 플래그를 바꾸면 다시 show()해야 반영된다
}

void cStickyNote::RefreshListView()
{
	if( _listModel == nullptr )
	{
		_listModel = new QStandardItemModel( this );
		ui.listView->setModel( _listModel );
	}

	_listModel->clear();

	QString notePath = Tools::GetSettingValue( "Default", "NoteSavePath", "" ).toString();
	QDir dir( notePath );

	QStringList files = dir.entryList( QStringList() << "*.txt", QDir::Files );

	for( const QString& fileName : files )
	{
		QString guid = QFileInfo( fileName ).completeBaseName();

		QString title = Tools::GetSettingValue( SettingGroup( guid ), "Title", "" ).toString();

		if( title.isEmpty() == true )
			title = "(제목 없음)";

		auto item = new QStandardItem( title );

		item->setData( guid, Qt::UserRole );
		item->setEditable( false );

		if( IsGuidOpen( guid ) == true )
			item->setEnabled( false );

		_listModel->appendRow( item );
	}

	_listModel->sort( 0 );
}

void cStickyNote::CloseForSession()
{
	SaveContent();
	SaveMeta();

	hide();
}

void cStickyNote::closeEvent( QCloseEvent* event )
{
	SaveContent();
	SaveMeta();

	// X 버튼으로 사용자가 명시적으로 닫을 때만 "열려있던 목록"에서 지운다.
	// 로그오프/재부팅 등 세션 종료 시 OS가 강제로 창을 닫을 때는 여기로도 들어오는데,
	// 그때 목록에서 지워버리면 다음 실행 때 노트가 복원되지 않고 매번 빈 노트가 새로 생긴다.
	if( _explicitClose == true )
	{
		s_openGuids.remove( _guid );
		PersistOpenGuids();
	}

	emit Closed( this );

	QWidget::closeEvent( event );
}

void cStickyNote::RequestDeleteNote( const QString& guid )
{
	if( IsGuidOpen( guid ) == true )
	{
		QMessageBox::information( this, "노트 삭제", "열려있는 노트는 삭제할 수 없습니다. 먼저 노트를 닫아주세요." );
		return;
	}

	QString title = Tools::GetSettingValue( SettingGroup( guid ), "Title", "" ).toString();

	if( title.isEmpty() == true )
		title = "(제목 없음)";

	auto reply = QMessageBox::question( this, "노트 삭제",
		QString( "'%1' 노트를 완전히 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다." ).arg( title ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No );

	if( reply != QMessageBox::Yes )
		return;

	DeleteNoteFromDisk( guid );
	RefreshListView();
}

bool cStickyNote::DeleteNoteFromDisk( const QString& guid )
{
	if( guid.isEmpty() == true || IsGuidOpen( guid ) == true )
		return false;

	QString path = Tools::GetSettingValue( "Default", "NoteSavePath", "" ).toString() + "/" + guid + ".txt";

	QFile::remove( path );

	Tools::RemoveSettingGroup( "Note_" + guid );

	return true;
}

void cStickyNote::moveEvent( QMoveEvent* event )
{
	_saveTimer->start();

	QWidget::moveEvent( event );
}

void cStickyNote::resizeEvent( QResizeEvent* event )
{
	_saveTimer->start();

	QWidget::resizeEvent( event );
}

bool cStickyNote::nativeEvent( const QByteArray& eventType, void* message, qintptr* result )
{
	MSG* msg = static_cast<MSG*>( message );

	if( msg->message == WM_NCHITTEST )
	{
		constexpr int borderWidth = 6;

		QPoint pos = mapFromGlobal( QPoint( GET_X_LPARAM( msg->lParam ), GET_Y_LPARAM( msg->lParam ) ) );

		bool onLeft   = pos.x() < borderWidth;
		bool onRight  = pos.x() > width() - borderWidth;
		bool onTop    = pos.y() < borderWidth;
		bool onBottom = pos.y() > height() - borderWidth;

		if( onTop == true && onLeft == true )       { *result = HTTOPLEFT;     return true; }
		if( onTop == true && onRight == true )      { *result = HTTOPRIGHT;    return true; }
		if( onBottom == true && onLeft == true )    { *result = HTBOTTOMLEFT;  return true; }
		if( onBottom == true && onRight == true )   { *result = HTBOTTOMRIGHT; return true; }
		if( onLeft == true )                        { *result = HTLEFT;       return true; }
		if( onRight == true )                       { *result = HTRIGHT;      return true; }
		if( onTop == true )                         { *result = HTTOP;        return true; }
		if( onBottom == true )                      { *result = HTBOTTOM;     return true; }

		// 상단바(frame_2)의 빈 공간이나 제목 라벨을 누르면 창 이동(타이틀바처럼)
		if( ui.frame_2->geometry().contains( pos ) == true )
		{
			QWidget* child = childAt( pos );

			if( child == ui.frame_2 || child == ui.lblTitle )
			{
				*result = HTCAPTION;
				return true;
			}
		}
	}

	return QWidget::nativeEvent( eventType, message, result );
}
