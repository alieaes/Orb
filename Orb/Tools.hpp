#pragma once

namespace Tools
{
	QString                     GetSettingPath();
	QString                     GetSettingFilePath();

	bool                        IsExistSettingFile();
	bool                        CreateSettingFiles();

	QVariant                    GetSettingValue( const QString& group, const QString& key, const QVariant& defaultValue = QVariant() );
	void                        SetSettingValue( const QString& group, const QString& key, const QVariant& value );
	void                        RemoveSettingGroup( const QString& group );

	// 노트 암호화 키는 디스크에 저장하지 않고 잠금 해제된 세션(메모리) 동안만 유지한다.
	// salt/verifier(비밀번호 검증용)만 DPAPI로 감싸서 settings.ini에 저장해둔다.
	bool                        HasNoteKey();
	void                        ClearNoteKey();

	bool                        SetupNoteKey( const QString& password );   // 암호화 최초 설정 (새 비밀번호)
	bool                        UnlockNoteKey( const QString& password );  // 기존 비밀번호로 잠금 해제
	void                        ForgetNoteKeySetup();                      // 암호화 완전 해제 (salt/verifier 삭제)

	QByteArray                  EncryptNote( const QString& plainText );
	QString                     DecryptNote( const QByteArray& cipherData, bool* ok = nullptr );
}
