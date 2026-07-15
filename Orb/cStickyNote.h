#pragma once

#include <QWidget>
#include <QSet>

#include "ui_cStickyNote.h"

class QStandardItemModel;

// 노트 하나(txt 파일 하나)를 보여주는 떠있는 창.
// 내용은 {NoteSavePath}/{guid}.txt 로, 위치/크기/핀/투명도/제목 같은 메타데이터는
// settings.ini의 [Note_{guid}] 그룹에 저장한다.
class cStickyNote : public QWidget
{
	Q_OBJECT

public:
	cStickyNote( QWidget* parent = nullptr );
	~cStickyNote();

	QString Guid() const { return _guid; }

	// 기존 노트를 불러온다 (내용 + 메타데이터)
	void LoadNote( const QString& guid );

	// 새 guid를 발급해서 빈 노트로 초기화하고 즉시 파일/메타데이터를 만든다
	void CreateNewNote();

	// 마스터 토글로 전체를 끌 때 사용 - 저장은 하되, "열려있던 목록"에서는 안 지운다
	// (다음에 다시 켰을 때 그대로 복원하기 위함). 실제 사용자 X 클릭(closeEvent)과는 다르다.
	void CloseForSession();

	static bool IsGuidOpen( const QString& guid );

	// 디스크에서 노트 파일 + 메타데이터를 완전히 삭제한다 (현재 열려있는 노트는 대상에서 제외)
	static bool DeleteNoteFromDisk( const QString& guid );

signals:
	void Closed( cStickyNote* self );
	void RequestNewNote();

protected:
	void closeEvent( QCloseEvent* event ) override;
	void moveEvent( QMoveEvent* event ) override;
	void resizeEvent( QResizeEvent* event ) override;

	// Qt::FramelessWindowHint라 OS가 주는 타이틀바 드래그/테두리 리사이즈가 없어짐 -
	// WM_NCHITTEST에 직접 응답해서 상단바는 드래그, 가장자리는 리사이즈로 동작하게 한다.
	bool nativeEvent( const QByteArray& eventType, void* message, qintptr* result ) override;

private:
	void SaveContent();
	void SaveMeta();
	void RefreshListView();
	void SwitchToNote( const QString& guid );
	void UpdateTitleFromContent();
	void ApplyPinned( bool pinned );
	void RequestDeleteNote( const QString& guid );
	QString NoteFilePath( const QString& guid ) const;
	QString SettingGroup( const QString& guid ) const;

	static void PersistOpenGuids();

	static QSet<QString> s_openGuids;

	Ui::cStickyNote      ui;

	QString              _guid;
	bool                 _isPinned         = false;
	bool                 _explicitClose    = false; // 사용자가 X 버튼을 눌러서 닫는 경우만 true - 세션 종료 등 외부에서 강제로 닫힐 때와 구분한다
	QTimer*              _saveTimer        = nullptr;
	QTimer*              _topmostTimer     = nullptr;
	QStandardItemModel*  _listModel        = nullptr;
};
