#include "stdafx.h"
#include "Tools.hpp"

#include <Windows.h>
#include <wincrypt.h>
#include <dpapi.h>

#include <sodium.h>

const QString SETTING_FILENAME    = "settings.ini";
const int     SETTING_DEFAULT_REV = 1;

namespace
{
    // Orb 전용 추가 엔트로피. CRYPTPROTECT_LOCAL_MACHINE만 쓰면 같은 컴퓨터의 아무 프로세스나
    // CryptUnprotectData를 호출해서 풀 수 있는데, 이 값을 같이 넘겨야만 풀리게 해서 문턱을 하나 더 둔다.
    // (exe를 리버싱하면 이 값도 뽑아낼 수 있긴 하지만, 최소한 API만 무작정 호출해서는 못 뚫는다)
    const BYTE ORB_DPAPI_ENTROPY[] =
    {
        0x4f, 0x72, 0x62, 0x2e, 0x4e, 0x6f, 0x74, 0x65,
        0x4b, 0x65, 0x79, 0x2e, 0x45, 0x6e, 0x74, 0x72,
        0x6f, 0x70, 0x79, 0x2e, 0x76, 0x31, 0xa4, 0x7c
    };

    DATA_BLOB EntropyBlob()
    {
        DATA_BLOB entropy{};
        entropy.pbData = ( BYTE* )ORB_DPAPI_ENTROPY;
        entropy.cbData = ( DWORD )sizeof( ORB_DPAPI_ENTROPY );
        return entropy;
    }

    // DPAPI 컴퓨터 단위 보호 (CRYPTPROTECT_LOCAL_MACHINE) : 특정 사용자 계정이 아니라
    // 이 컴퓨터 자체에 묶여서, 같은 컴퓨터의 어느 계정에서도 복호화가 가능하다.
    QByteArray DpapiProtect( const QByteArray& data )
    {
        DATA_BLOB in{};
        in.pbData = ( BYTE* )data.constData();
        in.cbData = ( DWORD )data.size();

        DATA_BLOB entropy = EntropyBlob();
        DATA_BLOB out{};

        if( CryptProtectData( &in, L"Orb.NoteKey", &entropy, nullptr, nullptr, CRYPTPROTECT_LOCAL_MACHINE, &out ) == FALSE )
            return QByteArray();

        QByteArray result( ( const char* )out.pbData, ( int )out.cbData );
        LocalFree( out.pbData );

        return result;
    }

    QByteArray DpapiUnprotect( const QByteArray& data )
    {
        DATA_BLOB in{};
        in.pbData = ( BYTE* )data.constData();
        in.cbData = ( DWORD )data.size();

        DATA_BLOB entropy = EntropyBlob();
        DATA_BLOB out{};

        if( CryptUnprotectData( &in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_LOCAL_MACHINE, &out ) == FALSE )
            return QByteArray();

        QByteArray result( ( const char* )out.pbData, ( int )out.cbData );
        LocalFree( out.pbData );

        return result;
    }

    // 노트 암호화 키(DEK)는 디스크에 절대 저장하지 않고, 잠금 해제된 세션 동안만 메모리에 들고 있는다.
    QByteArray g_sessionKey;

    // secretbox 원본 헬퍼 : 주어진 키로 직접 암/복호화 (verifier, 실제 노트 둘 다 이걸로 처리)
    QByteArray EncryptWithKey( const QByteArray& plain, const QByteArray& key )
    {
        QByteArray output( crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + plain.size(), 0 );

        unsigned char* nonce = ( unsigned char* )output.data();
        randombytes_buf( nonce, crypto_secretbox_NONCEBYTES );

        unsigned char* cipher = nonce + crypto_secretbox_NONCEBYTES;

        crypto_secretbox_easy( cipher,
                                ( const unsigned char* )plain.constData(), plain.size(),
                                nonce,
                                ( const unsigned char* )key.constData() );

        return output;
    }

    QString DecryptWithKey( const QByteArray& cipherData, const QByteArray& key, bool* ok )
    {
        if( ok != nullptr )
            *ok = false;

        if( cipherData.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES )
            return QString();

        const unsigned char* nonce = ( const unsigned char* )cipherData.constData();
        const unsigned char* cipher = nonce + crypto_secretbox_NONCEBYTES;
        int cipherLen = cipherData.size() - crypto_secretbox_NONCEBYTES;

        QByteArray plain( cipherLen - crypto_secretbox_MACBYTES, 0 );

        if( crypto_secretbox_open_easy( ( unsigned char* )plain.data(),
                                         cipher, cipherLen,
                                         nonce,
                                         ( const unsigned char* )key.constData() ) != 0 )
            return QString(); // 복호화 실패 (변조되었거나 키가 맞지 않음)

        if( ok != nullptr )
            *ok = true;

        return QString::fromUtf8( plain );
    }

    // 비밀번호 + salt로 32바이트 키를 유도 (Argon2, crypto_pwhash)
    QByteArray DeriveKeyFromPassword( const QString& password, const QByteArray& salt )
    {
        QByteArray key( crypto_secretbox_KEYBYTES, 0 );
        QByteArray pass = password.toUtf8();

        if( crypto_pwhash( ( unsigned char* )key.data(), key.size(),
                            pass.constData(), pass.size(),
                            ( const unsigned char* )salt.constData(),
                            crypto_pwhash_OPSLIMIT_INTERACTIVE,
                            crypto_pwhash_MEMLIMIT_INTERACTIVE,
                            crypto_pwhash_ALG_DEFAULT ) != 0 )
            return QByteArray(); // 메모리 부족 등

        return key;
    }

    const char* const NOTE_KEY_VERIFY_TEXT = "Orb-Note-Verify-v1";
}

QString Tools::GetSettingPath()
{
	QString programData = qEnvironmentVariable( "ProgramData" );

	if( programData.isEmpty() )
		programData = "C:\\ProgramData";

	return programData + "\\Orb";
}

QString Tools::GetSettingFilePath()
{
	return GetSettingPath() + "\\" + SETTING_FILENAME;
}

bool Tools::IsExistSettingFile()
{
	return QFile::exists( GetSettingFilePath() );
}

bool Tools::CreateSettingFiles()
{
	if( IsExistSettingFile() == true )
		return true;

	QDir().mkpath( GetSettingPath() );

	QString sDefaultNotePath = GetSettingPath() + "\\Notes";
	QDir().mkpath( sDefaultNotePath );

	QSettings setting( GetSettingFilePath(), QSettings::IniFormat );

	setting.beginGroup( "Default" );
	setting.setValue( "rev", SETTING_DEFAULT_REV );
	setting.setValue( "NoteSavePath", sDefaultNotePath );
	setting.setValue( "NoteEncrypt", "false" );
	setting.setValue( "NoteEncryptKey", "" );
	setting.endGroup();

	setting.sync();

	return setting.status() == QSettings::NoError;
}

QVariant Tools::GetSettingValue( const QString& group, const QString& key, const QVariant& defaultValue )
{
	QSettings setting( GetSettingFilePath(), QSettings::IniFormat );
	setting.beginGroup( group );
	QVariant value = setting.value( key, defaultValue );
	setting.endGroup();
	return value;
}

void Tools::SetSettingValue( const QString& group, const QString& key, const QVariant& value )
{
	QSettings setting( GetSettingFilePath(), QSettings::IniFormat );
	setting.beginGroup( group );
	setting.setValue( key, value );
	setting.endGroup();
}

bool Tools::HasNoteKey()
{
	return g_sessionKey.isEmpty() == false;
}

void Tools::ClearNoteKey()
{
	if( g_sessionKey.isEmpty() == false )
		sodium_memzero( g_sessionKey.data(), g_sessionKey.size() );

	g_sessionKey.clear();
}

bool Tools::SetupNoteKey( const QString& password )
{
	QByteArray salt( crypto_pwhash_SALTBYTES, 0 );
	randombytes_buf( ( unsigned char* )salt.data(), salt.size() );

	QByteArray key = DeriveKeyFromPassword( password, salt );

	if( key.isEmpty() == true )
		return false;

	QByteArray verifier = EncryptWithKey( QByteArray( NOTE_KEY_VERIFY_TEXT ), key );

	// salt/verifier 자체는 비밀은 아니지만(비밀번호 없이는 어차피 못 품), 최소한 파일을 그대로
	// 다른 컴퓨터로 옮기는 것만으로는 오프라인 브루트포스 시도조차 못 하게 DPAPI로 한 번 더 감싼다.
	QByteArray protectedSalt = DpapiProtect( salt );
	QByteArray protectedVerifier = DpapiProtect( verifier );

	SetSettingValue( "Default", "NoteKeySalt", QString::fromUtf8( protectedSalt.toBase64() ) );
	SetSettingValue( "Default", "NoteKeyVerifier", QString::fromUtf8( protectedVerifier.toBase64() ) );

	g_sessionKey = key;

	return true;
}

bool Tools::UnlockNoteKey( const QString& password )
{
	QString saltEncoded = GetSettingValue( "Default", "NoteKeySalt", "" ).toString();
	QString verifierEncoded = GetSettingValue( "Default", "NoteKeyVerifier", "" ).toString();

	if( saltEncoded.isEmpty() == true || verifierEncoded.isEmpty() == true )
		return false;

	QByteArray salt = DpapiUnprotect( QByteArray::fromBase64( saltEncoded.toUtf8() ) );
	QByteArray verifier = DpapiUnprotect( QByteArray::fromBase64( verifierEncoded.toUtf8() ) );

	if( salt.isEmpty() == true || verifier.isEmpty() == true )
		return false; // 다른 컴퓨터로 옮겨졌거나 손상됨

	QByteArray key = DeriveKeyFromPassword( password, salt );

	if( key.isEmpty() == true )
		return false;

	bool isVerified = false;
	DecryptWithKey( verifier, key, &isVerified );

	if( isVerified == false )
		return false; // 비밀번호가 틀림

	g_sessionKey = key;

	return true;
}

void Tools::ForgetNoteKeySetup()
{
	QSettings setting( GetSettingFilePath(), QSettings::IniFormat );

	setting.beginGroup( "Default" );
	setting.remove( "NoteKeySalt" );
	setting.remove( "NoteKeyVerifier" );
	setting.endGroup();

	ClearNoteKey();
}

QByteArray Tools::EncryptNote( const QString& plainText )
{
	if( HasNoteKey() == false )
		return QByteArray();

	return EncryptWithKey( plainText.toUtf8(), g_sessionKey );
}

QString Tools::DecryptNote( const QByteArray& cipherData, bool* ok )
{
	if( HasNoteKey() == false )
	{
		if( ok != nullptr )
			*ok = false;

		return QString();
	}

	return DecryptWithKey( cipherData, g_sessionKey, ok );
}
