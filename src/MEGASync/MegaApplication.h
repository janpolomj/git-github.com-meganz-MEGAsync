#ifndef MEGAAPPLICATION_H
#define MEGAAPPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDataStream>
#include <QQueue>
#include <QNetworkConfigurationManager>
#include <QNetworkInterface>

#include "gui/TransferManager.h"
#include "gui/NodeSelector.h"
#include "gui/InfoDialog.h"
#include "gui/InfoOverQuotaDialog.h"
#include "gui/SetupWizard.h"
#include "gui/SettingsDialog.h"
#include "gui/UploadToMegaDialog.h"
#include "gui/DownloadFromMegaDialog.h"
#include "gui/StreamingFromMegaDialog.h"
#include "gui/ImportMegaLinksDialog.h"
#include "gui/MultiQFileDialog.h"
#include "gui/PasteMegaLinksDialog.h"
#include "gui/ChangeLogDialog.h"
#include "gui/UpgradeDialog.h"
#include "gui/InfoWizard.h"
#include "control/Preferences.h"
#include "control/HTTPServer.h"
#include "control/MegaUploader.h"
#include "control/MegaDownloader.h"
#include "control/UpdateTask.h"
#include "control/MegaSyncLogger.h"
#include "megaapi.h"
#include "QTMegaListener.h"

#ifdef __APPLE__
    #include "gui/MegaSystemTrayIcon.h"
    #include <mach/mach.h>
#endif

Q_DECLARE_METATYPE(QQueue<QString>)

class Notificator;
class MEGASyncDelegateListener;

class MegaApplication : public QApplication, public mega::MegaListener
{
    Q_OBJECT

public:

    explicit MegaApplication(int &argc, char **argv);
    ~MegaApplication();

    void initialize();
    static QString applicationFilePath();
    static QString applicationDirPath();
    static QString applicationDataPath();
    void changeLanguage(QString languageCode);
    void updateTrayIcon();

    virtual void onRequestStart(mega::MegaApi* api, mega::MegaRequest *request);
    virtual void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e);
    virtual void onRequestTemporaryError(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError* e);
    virtual void onTransferStart(mega::MegaApi *api, mega::MegaTransfer *transfer);
    virtual void onTransferFinish(mega::MegaApi* api, mega::MegaTransfer *transfer, mega::MegaError* e);
    virtual void onTransferUpdate(mega::MegaApi *api, mega::MegaTransfer *transfer);
    virtual void onTransferTemporaryError(mega::MegaApi *api, mega::MegaTransfer *transfer, mega::MegaError* e);
    virtual void onAccountUpdate(mega::MegaApi *api);
    virtual void onUsersUpdate(mega::MegaApi* api, mega::MegaUserList *users);
    virtual void onNodesUpdate(mega::MegaApi* api, mega::MegaNodeList *nodes);
    virtual void onReloadNeeded(mega::MegaApi* api);
    virtual void onGlobalSyncStateChanged(mega::MegaApi *api);
    virtual void onSyncStateChanged(mega::MegaApi *api,  mega::MegaSync *sync);
    virtual void onSyncFileStateChanged(mega::MegaApi *api, mega::MegaSync *sync, const char *filePath, int newState);


    mega::MegaApi *getMegaApi() { return megaApi; }

    void unlink();
    void showInfoMessage(QString message, QString title = tr("MEGAsync"));
    void showWarningMessage(QString message, QString title = tr("MEGAsync"));
    void showErrorMessage(QString message, QString title = tr("MEGAsync"));
    void showNotificationMessage(QString message, QString title = tr("MEGAsync"));
    void setUploadLimit(int limit);
    void setMaxUploadSpeed(int limit);
    void setMaxDownloadSpeed(int limit);
    void setMaxConnections(int direction, int connections);
    void setUseHttpsOnly(bool httpsOnly);
    void startUpdateTask();
    void stopUpdateTask();
    void applyProxySettings();
    void updateUserStats();
    void addRecentFile(QString fileName, long long fileHandle, QString localPath = QString(), QString nodeKey = QString());
    void checkForUpdates();
    void showTrayMenu(QPoint *point = NULL);
    void toggleLogging();
    QList<mega::MegaTransfer* > getFinishedTransfers();
    int getNumUnviewedTransfers();
    void removeFinishedTransfer(int transferTag);
    void removeAllFinishedTransfers();
    mega::MegaTransfer* getFinishedTransferByTag(int tag);

signals:
    void startUpdaterThread();
    void tryUpdate();
    void installUpdate();
    void unityFixSignal();

public slots:
    void showInterface(QString);
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onMessageClicked();
    void start();
    void openSettings(int tab = SettingsDialog::ACCOUNT_TAB);
    void openInfoWizard();
    void openBwOverquotaDialog();
    void changeProxy();
    void importLinks();
    void showChangeLog();
    void uploadActionClicked();
    void loginActionClicked();
    void copyFileLink(mega::MegaHandle fileHandle, QString nodeKey = QString());
    void downloadActionClicked();
    void streamActionClicked();
    void transferManagerActionClicked();
    void logoutActionClicked();
    void processDownloads();
    void processUploads();
    void shellUpload(QQueue<QString> newUploadQueue);
    void shellExport(QQueue<QString> newExportQueue);
    void exportNodes(QList<mega::MegaHandle> exportList, QStringList extraLinks = QStringList());
    void externalDownload(QQueue<mega::MegaNode *> newDownloadQueue);
    void externalDownload(QString megaLink, QString auth);
    void internalDownload(long long handle);
    void syncFolder(long long handle);
    void onLinkImportFinished();
    void onRequestLinksFinished();
    void onUpdateCompleted();
    void onUpdateAvailable(bool requested);
    void onInstallingUpdate(bool requested);
    void onUpdateNotFound(bool requested);
    void onUpdateError();
    void rebootApplication(bool update = true);
    void exitApplication();
    void pauseTransfers(bool pause);
    void checkNetworkInterfaces();
    void checkMemoryUsage();
    void periodicTasks();
    void cleanAll();
    void onDupplicateLink(QString link, QString name, mega::MegaHandle handle);
    void onInstallUpdateClicked();
    void showInfoDialog();
    void triggerInstallUpdate();
    void scanningAnimationStep();
    void setupWizardFinished(int result);
    void overquotaDialogFinished(int result);
    void infoWizardDialogFinished(int result);
    void runConnectivityCheck();
    void onConnectivityCheckSuccess();
    void onConnectivityCheckError();
    void userAction(int action);
    void changeState();
    void showUpdatedMessage();
    void handleMEGAurl(const QUrl &url);
    void handleLocalPath(const QUrl &url);
    void clearViewedTransfers();
    void onCompletedTransfersTabActive(bool active);
    void checkFirstTransfer();

protected:
    void createTrayIcon();
    void createTrayMenu();
    void createOverQuotaMenu();
    void createGuestMenu();
    bool showTrayIconAlwaysNEW();
    void loggedIn();
    void startSyncs();
    void processUploadQueue(mega::MegaHandle nodeHandle);
    void processDownloadQueue(QString path);
    void unityFix();
    void disableSyncs();
    void restoreSyncs();
    void closeDialogs();
    void calculateInfoDialogCoordinates(QDialog *dialog, int *posx, int *posy);
    void deleteMenu(QMenu *menu);
    void printLinuxCmdHelp();
    void systrayIconPathsLinux(int iconTheme);

#ifdef Q_OS_LINUX
           const char* IconPathLoggin;
           const char* IconPathSyncin;
           const char* IconPathSynced;
           const char* IconPathPaused;
           const char* IconPathWarnin;
#endif

#ifdef __APPLE__
    MegaSystemTrayIcon *trayIcon;
#else
    QSystemTrayIcon *trayIcon;
#endif

    QAction *changeProxyAction;
    QAction *initialExitAction;
    QMenu *initialMenu;

#ifdef _WIN32
    QMenu *windowsMenu;
    QAction *windowsExitAction;
    QAction *windowsUpdateAction;
    QAction *windowsImportLinksAction;
    QAction *windowsUploadAction;
    QAction *windowsDownloadAction;
    QAction *windowsStreamAction;
    QAction *windowsTransferManagerAction;
    QAction *windowsSettingsAction;
#endif

    QMenu *trayMenu;
    QMenu *trayOverQuotaMenu;
    QMenu *trayGuestMenu;
    QMenu emptyMenu;
    QAction *exitAction;
    QAction *settingsAction;
    QAction *importLinksAction;
    QAction *uploadAction;
    QAction *downloadAction;
    QAction *streamAction;

    QAction *updateAction;
    QAction *showStatusAction;

    QAction *logoutActionOverquota;
    QAction *settingsActionOverquota;
    QAction *exitActionOverquota;
    QAction *updateActionOverquota;

    QAction *importLinksActionGuest;
    QAction *exitActionGuest;
    QAction *settingsActionGuest;
    QAction *updateActionGuest;
    QAction *loginActionGuest;

#ifdef __APPLE__
    QTimer *scanningTimer;
#endif

    QTimer *connectivityTimer;
    int scanningAnimationIndex;
    SetupWizard *setupWizard;
    SettingsDialog *settingsDialog;
    InfoDialog *infoDialog;
    InfoOverQuotaDialog *infoOverQuota;
    Preferences *preferences;
    mega::MegaApi *megaApi;
    mega::MegaApi *megaApiFolders;
    HTTPServer *httpServer;
    UploadToMegaDialog *uploadFolderSelector;
    DownloadFromMegaDialog *downloadFolderSelector;
    QPointer<StreamingFromMegaDialog> streamSelector;
    MultiQFileDialog *multiUploadFileDialog;
    QQueue<QString> uploadQueue;
    QQueue<mega::MegaNode *> downloadQueue;
    int numTransfers[2];
    unsigned int activeTransferTag[2];
    unsigned long long activeTransferPriority[2];
    unsigned int activeTransferState[2];
    long long queuedUserStats;
    long long maxMemoryUsage;
    int exportOps;
    int syncState;
    mega::MegaPricing *pricing;
    long long bwOverquotaTimestamp;
    bool enablingBwOverquota;
    UpgradeDialog *bwOverquotaDialog;
    bool bwOverquotaEvent;
    InfoWizard *infoWizard;
    mega::QTMegaListener *delegateListener;
    MegaUploader *uploader;
    MegaDownloader *downloader;
    QTimer *periodicTasksTimer;
    QTimer *infoDialogTimer;
    QTimer *firstTransferTimer;
    QTranslator translator;
    PasteMegaLinksDialog *pasteMegaLinksDialog;
    ChangeLogDialog *changeLogDialog;
    ImportMegaLinksDialog *importDialog;
    QMessageBox *exitDialog;
    QMessageBox *sslKeyPinningError;
    NodeSelector *downloadNodeSelector;
    QString lastTrayMessage;
    QStringList extraLinks;

    static QString appPath;
    static QString appDirPath;
    static QString dataPath;

    QThread *updateThread;
    UpdateTask *updateTask;
    Notificator *notificator;
    long long lastActiveTime;
    QNetworkConfigurationManager networkConfigurationManager;
    QList<QNetworkInterface> activeNetworkInterfaces;
    QMap<QString, QString> pendingLinks;
    MegaSyncLogger *logger;
    QPointer<TransferManager> transferManager;
    QMap<int, mega::MegaTransfer*> finishedTransfers;
    QList<mega::MegaTransfer*> finishedTransferOrder;

    bool reboot;
    bool syncActive;
    bool paused;
    bool indexing;
    bool waiting;
    bool updated;
    bool checkupdate;
    bool updateBlocked;
    long long lastExit;
    bool appfinished;
    bool updateAvailable;
    bool isLinux;
    long long externalNodesTimestamp;
    bool overquotaCheck;
    int noKeyDetected;
    bool isFirstSyncDone;
    bool isFirstFileSynced;
    bool networkConnectivity;
    int nUnviewedTransfers;
    bool completedTabActive;
};

class MEGASyncDelegateListener: public mega::QTMegaListener
{
public:
    MEGASyncDelegateListener(mega::MegaApi *megaApi, mega::MegaListener *parent=NULL);
    virtual void onRequestFinish(mega::MegaApi* api, mega::MegaRequest *request, mega::MegaError* e);
};

#endif // MEGAAPPLICATION_H
