#pragma once
#include <QJsonObject>
#include <QWidget>
#include <functional>
class Library;
class BuildModel;
class QListWidget;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QMediaPlayer;
class QHideEvent;

// Cursor's shared AudioAsset vocabulary, stored in the existing channel document.
struct AudioAsset {
    QString id,name,file;
    int seconds=8;
    double speed=1,fadeIn=0,fadeOut=0,echo=0,offset=0;
};
namespace AudioLibrary {
inline constexpr auto mimeType="application/x-cuebase-audio";
QJsonObject asset(const Library &library,const QString &id);
QString addToScene(BuildModel &model,const Library &library,const QString &id);
QString addToTimeline(Library &library,const QString &id,qint64 start,int layer,QString *error=nullptr);
QJsonObject placementSettings(const Library &library,const QString &id);
}

class AudioLibraryPanel final:public QWidget {
    Q_OBJECT
public:
    AudioLibraryPanel(Library *library,BuildModel *model=nullptr,QWidget *parent=nullptr);
    void selectPlacement(const QString &id);
    void importFile(const QString &path);
protected:
    void hideEvent(QHideEvent *event) override;
private:
    void stopPreview();
    void refresh();
    void loadControls();
    void saveControls(QDoubleSpinBox *changed);
    Library *m_library;
    BuildModel *m_model;
    QListWidget *m_assets,*m_scene=nullptr;
    QLabel *m_context;
    QPushButton *m_removeScene=nullptr;
    QPushButton *m_preview=nullptr;
    QMediaPlayer *m_previewPlayer=nullptr;
    QDoubleSpinBox *m_speed,*m_fadeIn,*m_fadeOut,*m_offset,*m_volume;
    QDoubleSpinBox *m_start=nullptr;
    QCheckBox *m_mute=nullptr;
    QString m_assetId,m_sceneId,m_placementId;
    bool m_syncing=false;
};
