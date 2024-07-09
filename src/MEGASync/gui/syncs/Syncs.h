#ifndef SYNCS_H
#define SYNCS_H

#include "syncs/control/SyncController.h"

#include "megaapi.h"
#include "mega/bindings/qt/QTMegaRequestListener.h"

#include <QObject>

#include <memory>

class SyncController;
class Syncs : public QObject, public mega::MegaRequestListener
{
    Q_OBJECT

    Q_PROPERTY(QString defaultMegaFolder READ getDefaultMegaFolder CONSTANT FINAL)
    Q_PROPERTY(QString defaultMegaPath READ getDefaultMegaPath CONSTANT FINAL)
    Q_PROPERTY(SyncStatusCode syncStatus READ getSyncStatus WRITE setSyncStatus NOTIFY syncStatusChanged)

public:
    enum SyncStatusCode
    {
        NONE = 0,
        FULL,
        SELECTIVE
    };
    Q_ENUM(SyncStatusCode)

    static const QString DEFAULT_MEGA_FOLDER;
    static const QString DEFAULT_MEGA_PATH;

    Syncs(QObject* parent = nullptr);
    virtual ~Syncs() = default;

    Q_INVOKABLE void addSync(const QString& local, const QString& remote = QLatin1String("/"));
    Q_INVOKABLE bool checkLocalSync(const QString& path) const;
    Q_INVOKABLE bool checkRemoteSync(const QString& path) const;

    QString getDefaultMegaFolder() const;
    QString getDefaultMegaPath() const;

    SyncStatusCode getSyncStatus() const;
    void setSyncStatus(SyncStatusCode status);

signals:
    void syncSetupSuccess();
    void cantSync(const QString& message = QString(), bool localFolderError = true);
    void syncStatusChanged();
    void syncRemoved();

private:
    mega::MegaApi* mMegaApi;
    std::unique_ptr<mega::QTMegaRequestListener> mDelegateListener;
    std::unique_ptr<SyncController> mSyncController;
    QString mRemoteFolder;
    QString mLocalFolder;
    bool mCreatingFolder;
    SyncStatusCode mSyncStatus;

    bool errorOnSyncPaths(const QString& localPath, const QString& remotePath);
    bool helperCheckLocalSync(const QString& path, QString& errorMessage) const;
    bool helperCheckRemoteSync(const QString& path, QString& errorMessage) const;

private slots:
    void onSyncAddRequestStatus(int errorCode, int syncErrorCode, QString name);
    void onRequestFinish(mega::MegaApi* api, mega::MegaRequest* request, mega::MegaError* e) override;
    void onSyncRemoved(std::shared_ptr<SyncSettings> syncSettings);

};

#endif // SYNCS_H
