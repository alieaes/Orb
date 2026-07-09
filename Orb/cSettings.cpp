#include "stdafx.h"
#include "cSettings.h"

#include "Tools.hpp"

#include <QSaveFile>
#include <QInputDialog>
#include <functional>

namespace
{
	// 새 비밀번호를 두 번 입력받아 일치하는지 확인한다 (오타로 스스로 잠기는 걸 방지).
	// 취소하거나 두 번 입력이 다르면 빈 문자열.
	QString PromptNewPassword( QWidget* parent )
	{
		bool ok = false;

		QString pw = QInputDialog::getText( parent, "노트 암호화",
		    "노트를 암호화할 비밀번호를 입력하세요.", QLineEdit::Password, "", &ok );

		if( ok == false || pw.isEmpty() == true )
			return QString();

		QString pw2 = QInputDialog::getText( parent, "노트 암호화",
		    "비밀번호를 한 번 더 입력하세요.", QLineEdit::Password, "", &ok );

		if( ok == false || pw2 != pw )
		{
			QMessageBox::warning( parent, "노트 암호화", "비밀번호가 일치하지 않습니다." );
			return QString();
		}

		return pw;
	}

	QString PromptExistingPassword( QWidget* parent, const QString& label )
	{
		bool ok = false;

		QString pw = QInputDialog::getText( parent, "노트 암호화", label, QLineEdit::Password, "", &ok );

		if( ok == false || pw.isEmpty() == true )
			return QString();

		return pw;
	}
}

cSettings::cSettings( QWidget* parent, Qt::WindowFlags f )
	: QWidget( parent, f )
{
	ui.setupUi( this );

	ui.frNoteEncryptCode->hide();

	ui.btnClose->setIcon( QIcon( ":/cOrb/icons/close.svg" ) );
	ui.btnClose->setIconSize( QSize( 14, 14 ) );
	ui.btnClose->setFixedSize( 22, 22 );
	ui.btnClose->setCursor( Qt::PointingHandCursor );

	ui.btnClose->setStyleSheet(
	    "QPushButton {"
	    "  border: 1px solid transparent;"
	    "  border-radius: 4px;"
	    "  background: transparent;"
	    "}"
	    "QPushButton:hover {"
	    "  border: 1px solid rgba(255, 255, 255, 120);"
	    "  background: rgba(255, 255, 255, 25);"
	    "}"
	    "QPushButton:pressed {"
	    "  border: 1px solid rgba(255, 90, 90, 200);"
	    "  background: rgba(255, 90, 90, 45);"
	    "}" );

	connect( ui.btnClose, &QPushButton::clicked, this, &QWidget::close );

	ui.edtsaveLocation->setText( Tools::GetSettingValue( "Default", "NoteSavePath", "" ).toString() );

	// Designer가 모르는 커스텀 위젯이라 .ui에는 placeholder QWidget만 넣어뒀고, 실제 Switch로 교체한다
	auto swNoteEncrypt = new Switch( this );

	if( auto layout = ui.frNoteEncrypt->layout() )
	{
		if( auto oldItem = layout->replaceWidget( ui.swNoteEncrypt, swNoteEncrypt ) )
			delete oldItem;
	}

	delete ui.swNoteEncrypt;
	ui.swNoteEncrypt = swNoteEncrypt;

	bool isEncryptOn = Tools::GetSettingValue( "Default", "NoteEncrypt", false ).toBool();

	swNoteEncrypt->setChecked( isEncryptOn );
	//ui.frNoteEncryptCode->setVisible( isEncryptOn );

	connect( swNoteEncrypt, &Switch::stateChanged, this, [ this, swNoteEncrypt ] ( int state )
	{
		bool wantEncrypt = ( state == Qt::Checked );
		bool isSuccess = false;

		if( wantEncrypt == true )
		{
			// 새로 켜는 거니까 이번에 쓸 비밀번호를 새로 만든다
			QString password = PromptNewPassword( this );

			if( password.isEmpty() == false && Tools::SetupNoteKey( password ) == true )
				isSuccess = NoteEncrypt();
		}
		else
		{
			// 끄려면(=평문으로 되돌리려면) 기존 비밀번호로 잠금을 풀 수 있어야 한다.
			// 이번 세션에서 이미 풀려있는 상태(스티키노트 토글로 이미 입력함)면 다시 안 물어본다.
			if( Tools::HasNoteKey() == false )
			{
				QString password = PromptExistingPassword( this, "노트 암호화를 해제하려면 비밀번호를 입력하세요." );

				if( password.isEmpty() == false )
					Tools::UnlockNoteKey( password );
			}

			if( Tools::HasNoteKey() == true )
			{
				isSuccess = NoteDecrypt();

				if( isSuccess == true )
					Tools::ForgetNoteKeySetup();
			}
		}

		if( isSuccess == false )
		{
			QMessageBox::warning( this, "노트 암호화",
			    wantEncrypt ? "노트 폴더를 암호화하는 중 문제가 발생했습니다." : "노트 폴더를 복호화하는 중 문제가 발생했습니다." );

			// 실패했으니 스위치를 원래 상태로 되돌린다.
			// 지금 이 슬롯은 Switch::checkStateSet()이 emit stateChanged(state) 하는 도중에 동기 호출된 것이고,
			// checkStateSet()은 emit 직후에 toggle(state)를 그 자리에서 다시 호출한다(state는 지역변수로 캡처된 값).
			// 그래서 여기서 곧바로 setChecked()를 불러도, 우리가 리턴한 뒤 바깥쪽 checkStateSet()이 이어서
			// toggle(state)를 원래 값으로 다시 호출해버려 화면이 도로 원상태로 보인다.
			// 이번 호출 스택이 다 풀린 다음(다음 이벤트 루프 틱)으로 미뤄서 되돌린다.
			QTimer::singleShot( 0, this, [ swNoteEncrypt, wantEncrypt ] ()
			{
				swNoteEncrypt->blockSignals( true );
				swNoteEncrypt->setChecked( !wantEncrypt );
				swNoteEncrypt->blockSignals( false );
			} );

			return;
		}

		Tools::SetSettingValue( "Default", "NoteEncrypt", wantEncrypt );
		//ui.frNoteEncryptCode->setVisible( wantEncrypt );
	} );
}

cSettings::~cSettings()
{}

namespace
{
	// 노트 폴더의 파일 하나를 읽어서 transform(원문 바이트) 결과를 그 파일에 안전하게(임시파일 경유) 덮어쓴다
	bool TransformNoteFile( const QString& filePath, const std::function<bool( const QByteArray&, QByteArray& )>& transform )
	{
		QFile file( filePath );

		if( file.open( QIODevice::ReadOnly ) == false )
			return false;

		QByteArray input = file.readAll();
		file.close();

		QByteArray output;

		if( transform( input, output ) == false )
			return false;

		QSaveFile saveFile( filePath );

		if( saveFile.open( QIODevice::WriteOnly ) == false )
			return false;

		saveFile.write( output );

		return saveFile.commit();
	}
}

bool cSettings::NoteEncrypt()
{
	if( Tools::HasNoteKey() == false )
		return false; // 세션에 키가 잠금 해제되어 있지 않으면 절대 진행하지 않는다

	QString sNotePath = Tools::GetSettingValue( "Default", "NoteSavePath", "" ).toString();

	if( sNotePath.isEmpty() == true )
		return false;

	QDir dir( sNotePath );

	if( dir.exists() == false )
		return true; // 암호화할 노트가 없으면 성공으로 취급

	QStringList files = dir.entryList( QDir::Files );

	for( const QString& fileName : files )
	{
		bool isSuccess = TransformNoteFile( dir.filePath( fileName ),
		    [] ( const QByteArray& input, QByteArray& output )
		{
			output = Tools::EncryptNote( QString::fromUtf8( input ) );
			return output.isEmpty() == false; // 빈 결과 = 세션 키 없음(실패)
		} );

		if( isSuccess == false )
			return false;
	}

	return true;
}

bool cSettings::NoteDecrypt()
{
	if( Tools::HasNoteKey() == false )
		return false; // 세션에 키가 잠금 해제되어 있지 않으면 절대 진행하지 않는다

	QString sNotePath = Tools::GetSettingValue( "Default", "NoteSavePath", "" ).toString();

	if( sNotePath.isEmpty() == true )
		return false;

	QDir dir( sNotePath );

	if( dir.exists() == false )
		return true; // 복호화할 노트가 없으면 성공으로 취급

	QStringList files = dir.entryList( QDir::Files );

	for( const QString& fileName : files )
	{
		bool isSuccess = TransformNoteFile( dir.filePath( fileName ),
		    [] ( const QByteArray& input, QByteArray& output )
		{
			bool ok = false;
			QString plainText = Tools::DecryptNote( input, &ok );

			if( ok == false )
				return false; // 잘못된 키/변조된 파일 - 원본을 보존하고 중단

			output = plainText.toUtf8();
			return true;
		} );

		if( isSuccess == false )
			return false;
	}

	return true;
}
