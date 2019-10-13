#include <QDesktopServices>
#include <QDesktopWidget>
#include <QUrl>
#include <QRect>
#include <QTimer>
#include <QHelpEvent>
#include <QToolTip>
#include <QSignalMapper>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QEvent>
#include "InfoDialog.h"
#include "ui_InfoDialog.h"
#include "control/Utilities.h"
#include "MegaApplication.h"
#include "MenuItemAction.h"
#include "platform/Platform.h"

#ifdef _WIN32    
#include <chrono>
using namespace std::chrono;
#endif

#if QT_VERSION >= 0x050000
#include <QtConcurrent/QtConcurrent>
#endif

using namespace mega;

InfoDialog::InfoDialog(MegaApplication *app, QWidget *parent, InfoDialog* olddialog) :
    QDialog(parent),
    ui(new Ui::InfoDialog)
{
    ui->setupUi(this);
    //Set window properties
#ifdef Q_OS_LINUX
    doNotActAsPopup = false;
    if (getenv("USE_MEGASYNC_AS_REGULAR_WINDOW"))
    {
        doNotActAsPopup = true;
    }

    if (!doNotActAsPopup && QSystemTrayIcon::isSystemTrayAvailable())
    {
        setWindowFlags(Qt::FramelessWindowHint); //To avoid issues with text input we implement a popup (instead of using Qt::Popup) ourselves by listening to WindowDeactivate event
    }
    else
    {
        setWindowFlags(Qt::Window);
        doNotActAsPopup = true; //the first time systray is not available will set this flag to true to disallow popup until restarting
    }
#else
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
#endif

#ifdef __APPLE__
    setAttribute(Qt::WA_TranslucentBackground);
#endif

    //Initialize fields
    this->app = app;
    activeDownloadState = activeUploadState = MegaTransfer::STATE_NONE;
    remainingUploads = remainingDownloads = 0;
    indexing = false;
    waiting = false;
    activeDownload = NULL;
    activeUpload = NULL;
    transferMenu = NULL;
    cloudItem = NULL;
    inboxItem = NULL;
    sharesItem = NULL;
    syncsMenu = NULL;
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    menuSignalMapper = NULL;
#endif
    rubbishItem = NULL;
    gWidget = NULL;
    opacityEffect = NULL;
    animation = NULL;

    actualAccountType = -1;

    overQuotaState = false;
    storageState = Preferences::STATE_BELOW_OVER_STORAGE;

    ui->lSDKblock->setText(QString::fromUtf8(""));
    ui->wBlocked->setVisible(false);
    ui->wContainerBottom->setFixedHeight(120);

    //Initialize header dialog and disable chat features
    ui->wHeader->setStyleSheet(QString::fromUtf8("#wHeader {border: none;}"));

    //Set properties of some widgets
    ui->sActiveTransfers->setCurrentWidget(ui->pUpdated);
    ui->pUsageStorage->setAttribute(Qt::WA_TransparentForMouseEvents);

#ifdef __APPLE__
    if (QSysInfo::MacintoshVersion <= QSysInfo::MV_10_9) //Issues with mavericks and popup management
    {
        installEventFilter(this);
    }
#endif

#ifdef Q_OS_LINUX
    installEventFilter(this);
#endif
    ui->wUsageStorage->installEventFilter(this);
    ui->wUsageStorage->setMouseTracking(true);

    ui->lOQDesc->setTextFormat(Qt::RichText);

    state = STATE_STARTING;
    ui->wStatus->setState(state);

    megaApi = app->getMegaApi();
    preferences = Preferences::instance();

    actualAccountType = -1;

    uploadsFinishedTimer.setSingleShot(true);
    uploadsFinishedTimer.setInterval(5000);
    connect(&uploadsFinishedTimer, SIGNAL(timeout()), this, SLOT(onAllUploadsFinished()));

    downloadsFinishedTimer.setSingleShot(true);
    downloadsFinishedTimer.setInterval(5000);
    connect(&downloadsFinishedTimer, SIGNAL(timeout()), this, SLOT(onAllDownloadsFinished()));

    transfersFinishedTimer.setSingleShot(true);
    transfersFinishedTimer.setInterval(5000);
    connect(&transfersFinishedTimer, SIGNAL(timeout()), this, SLOT(onAllTransfersFinished()));

    connect(ui->wStatus, SIGNAL(clicked()), app, SLOT(pauseTransfers()), Qt::QueuedConnection);
    connect(ui->wPSA, SIGNAL(PSAseen(int)), app, SLOT(PSAseen(int)), Qt::QueuedConnection);

    ui->bDotUsedQuota->hide();
    ui->bDotUsedStorage->hide();
    ui->sUsedData->setCurrentWidget(ui->pStorage);


    ui->wListTransfers->setupTransfers(olddialog?olddialog->stealModel():nullptr);



#ifdef __APPLE__
    arrow = new QPushButton(this);
    arrow->setIcon(QIcon(QString::fromLatin1("://images/top_arrow.png")));
    arrow->setIconSize(QSize(30,10));
    arrow->setStyleSheet(QString::fromLatin1("border: none;"));
    arrow->resize(30,10);
    arrow->hide();

    dummy = NULL;
#endif

    on_bDotUsedStorage_clicked();

    //Create the overlay widget with a semi-transparent background
    //that will be shown over the transfers when they are paused
    overlay = new QPushButton(this);
    overlay->setStyleSheet(QString::fromLatin1("background-color: transparent; "
                                              "border: none; "));
    overlay->resize(ui->pUpdated->size());
    overlay->setCursor(Qt::PointingHandCursor);

#ifdef __APPLE__
    overlay->move(1, 72);
#else
    overlay->move(2, 60);
    overlay->resize(overlay->width()-4, overlay->height());
#endif
    overlay->show();
    connect(overlay, SIGNAL(clicked()), this, SLOT(onOverlayClicked()));
    connect(this, SIGNAL(openTransferManager(int)), app, SLOT(externalOpenTransferManager(int)));

    if (preferences->logged())
    {
        setUsage();
        updateSyncsButton();
    }
    else
    {
        regenerateLayout(olddialog);
    }
    highDpiResize.init(this);

#ifdef _WIN32
    lastWindowHideTime = std::chrono::steady_clock::now() - 5s;

    PSA_info *psaData = olddialog ? olddialog->getPSAdata() : nullptr;
    if (psaData)
    {
        this->setPSAannouncement(psaData->idPSA, psaData->title, psaData->desc,
                                 psaData->urlImage, psaData->textButton, psaData->urlClick);
        delete psaData;
    }
#endif
}

InfoDialog::~InfoDialog()
{
    if (syncsMenu)
    {
        QList<QAction *> actions = syncsMenu->actions();
        for (int i = 0; i < actions.size(); i++)
        {
            syncsMenu->removeAction(actions[i]);
            delete actions[i];
        }
    }

    delete ui;
    delete gWidget;
    delete activeDownload;
    delete activeUpload;
    delete animation;
}

PSA_info *InfoDialog::getPSAdata()
{
    if (ui->wPSA->isPSAshown())
    {
        PSA_info* info = new PSA_info(ui->wPSA->getPSAdata());
        return info;
    }

    return nullptr;
}

void InfoDialog::hideEvent(QHideEvent *event)
{
#ifdef __APPLE__
    arrow->hide();
#endif

    QDialog::hideEvent(event);

#ifdef _WIN32
    lastWindowHideTime = std::chrono::steady_clock::now();
#endif
}

void InfoDialog::setAvatar()
{
    const char *email = megaApi->getMyEmail();
    if (email)
    {
        drawAvatar(QString::fromUtf8(email));
        delete [] email;
    }
}

void InfoDialog::setUsage()
{
    int accType = preferences->accountType();
    if (accType == Preferences::ACCOUNT_TYPE_FREE)
    {
        ui->bDotUsedQuota->hide();
        ui->bDotUsedStorage->hide();
    }
    else
    {
        ui->bDotUsedQuota->show();
        ui->bDotUsedStorage->show();
    }

    if (preferences->totalStorage() == 0)
    {
        ui->pUsageStorage->setValue(0);
        ui->lPercentageUsedStorage->setText(QString::fromUtf8(""));
        ui->lTotalUsedStorage->setText(QString::fromUtf8(""));
        ui->pUsageStorage->setProperty("crossedge", false);
        ui->pUsageStorage->setProperty("almostoq", false);
        ui->pUsageStorage->style()->unpolish(ui->pUsageStorage);
        ui->pUsageStorage->style()->polish(ui->pUsageStorage);
    }
    else
    {
        auto percentage = Utilities::percentage(preferences->usedStorage() , preferences->totalStorage());
        ui->pUsageStorage->setValue(percentage);

        if (percentage >= 100)
        {
            ui->pUsageStorage->setProperty("almostoq", false);
            ui->pUsageStorage->setProperty("crossedge", true);
        }
        else if (percentage > 90)
        {
            ui->pUsageStorage->setProperty("crossedge", false);
            ui->pUsageStorage->setProperty("almostoq", true);
        }
        else
        {
            ui->pUsageStorage->setProperty("crossedge", false);
            ui->pUsageStorage->setProperty("almostoq", false);
        }

        ui->pUsageStorage->style()->unpolish(ui->pUsageStorage);
        ui->pUsageStorage->style()->polish(ui->pUsageStorage);

        QString used = tr("%1 of %2").arg(QString::fromUtf8("<span style=\"color:#333333; font-size: 16px; text-decoration:none;\">%1</span>")
                                     .arg(QString::number(percentage > 100 ? 100 : percentage).append(QString::fromLatin1("%"))))
                                     .arg(QString::fromUtf8("<span style=\"color:#333333; font-size: 16px; text-decoration:none;\">&nbsp;%1</span>")
                                     .arg(Utilities::getSizeString(preferences->totalStorage())));
        ui->lPercentageUsedStorage->setText(used);
        ui->lTotalUsedStorage->setText(tr("USED STORAGE %1").arg(QString::fromUtf8("<span style=\"color:#333333; font-size: 16px; text-decoration:none;\">&nbsp;&nbsp;%1</span>")
                                       .arg(Utilities::getSizeString(preferences->usedStorage()))));
    }

    if (preferences->totalBandwidth() == 0)
    {
        ui->pUsageQuota->setValue(0);
        ui->lPercentageUsedQuota->setText(QString::fromUtf8(""));
        ui->lTotalUsedQuota->setText(tr("TRANSFER QUOTA %1").arg(tr("Data temporarily unavailable")));
    }
    else
    {
        auto percentage = Utilities::percentage(preferences->usedBandwidth(),preferences->totalBandwidth());
        ui->pUsageQuota->setValue(percentage);
        if (preferences->usedBandwidth() > preferences->totalBandwidth())
        {
            ui->pUsageQuota->setProperty("crossedge", true);
        }
        else
        {
            ui->pUsageQuota->setProperty("crossedge", false);
        }
        ui->pUsageQuota->style()->unpolish(ui->pUsageQuota);
        ui->pUsageQuota->style()->polish(ui->pUsageQuota);

        QString used = tr("%1 of %2").arg(QString::fromUtf8("<span style=\"color:#333333; font-size: 16px; text-decoration:none;\">%1&nbsp;</span>")
                                     .arg(QString::number(percentage > 100 ? 100 : percentage).append(QString::fromLatin1("%"))))
                                     .arg(QString::fromUtf8("<span style=\"color:#333333; font-size: 16px; text-decoration:none;\">&nbsp;%1</span>")
                                     .arg(Utilities::getSizeString(preferences->totalBandwidth())));
        ui->lPercentageUsedQuota->setText(used);
        ui->lTotalUsedQuota->setText(tr("TRANSFER QUOTA %1").arg(QString::fromUtf8("<span style=\"color:#333333; font-size: 16px; text-decoration:none;\">&nbsp;&nbsp;%1</span>")
                                                              .arg(Utilities::getSizeString(preferences->usedBandwidth()))));
    }
}

void InfoDialog::setTransfer(MegaTransfer *transfer)
{
    if (!transfer)
    {
        return;
    }

    int type = transfer->getType();
    if (type == MegaTransfer::TYPE_DOWNLOAD)
    {
        if (!activeDownload || activeDownload->getTag() != transfer->getTag())
        {
            ui->wListTransfers->getModel()->updateActiveTransfer(megaApi, transfer);

            delete activeDownload;
            activeDownload = transfer->copy();
        }
    }
    else
    {
        if (!activeUpload || activeUpload->getTag() != transfer->getTag())
        {
            ui->wListTransfers->getModel()->updateActiveTransfer(megaApi, transfer);

            delete activeUpload;
            activeUpload = transfer->copy();
        }
    }

    ui->wListTransfers->getModel()->onTransferUpdate(megaApi, transfer);
}

void InfoDialog::refreshTransferItems()
{
    if (ui->wListTransfers->getModel())
    {
        ui->wListTransfers->getModel()->refreshTransfers();
    }
}

void InfoDialog::transferFinished(int error)
{
    remainingUploads = megaApi->getNumPendingUploads();
    remainingDownloads = megaApi->getNumPendingDownloads();

    if (!remainingDownloads)
    {
        if (!downloadsFinishedTimer.isActive())
        {
            if (!error)
            {
                downloadsFinishedTimer.start();
            }
            else
            {
                onAllDownloadsFinished();
            }
        }
    }
    else
    {
        downloadsFinishedTimer.stop();
    }

    if (!remainingUploads)
    {
        if (!uploadsFinishedTimer.isActive())
        {
            if (!error)
            {
                uploadsFinishedTimer.start();
            }
            else
            {
                onAllUploadsFinished();
            }
        }
    }
    else
    {
        uploadsFinishedTimer.stop();
    }
}

void InfoDialog::updateSyncsButton()
{
    if (!preferences->logged())
    {
        return;
    }

    MegaNode *rootNode = megaApi->getRootNode();
    if (!rootNode)
    {
        ui->bSyncFolder->setText(QString::fromLatin1("MEGA"));
        return;
    }

    int num = preferences->getNumSyncedFolders();
    if (num == 0)
    {
        ui->bSyncFolder->setText(tr("Add Sync"));
    }
    else
    {
        bool found = false;
        for (int i = 0; i < num; i++)
        {
            if (preferences->isFolderActive(i))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ui->bSyncFolder->setText(tr("Add Sync"));
        }
        else
        {
            ui->bSyncFolder->setText(tr("Syncs"));
        }
    }

    delete rootNode;
}

void InfoDialog::setIndexing(bool indexing)
{
    this->indexing = indexing;
}

void InfoDialog::setWaiting(bool waiting)
{
    this->waiting = waiting;
}

void InfoDialog::setOverQuotaMode(bool state)
{
    if (overQuotaState == state)
    {
        return;
    }

    overQuotaState = state;
    ui->wStatus->setOverQuotaState(state);

    if (state)
    {
        ui->pUsageStorage->setProperty("overquota", true);
        ui->pUsageStorage->style()->unpolish(ui->pUsageStorage);
        ui->pUsageStorage->style()->polish(ui->pUsageStorage);
    }
    else
    {
        ui->pUsageStorage->setProperty("overquota", false);
        ui->pUsageStorage->style()->unpolish(ui->pUsageStorage);
        ui->pUsageStorage->style()->polish(ui->pUsageStorage);
    }
}

void InfoDialog::setAccountType(int accType)
{
    if (actualAccountType == accType)
    {
        return;
    }

    actualAccountType = accType;
    if (actualAccountType == Preferences::ACCOUNT_TYPE_BUSINESS)
    {
         ui->lTotalUsedStorage->installEventFilter(this);
         ui->lTotalUsedStorage->setMouseTracking(true);
         ui->wUsageStorage->removeEventFilter(this);
         ui->wUsageStorage->setMouseTracking(false);
         ui->bUpgrade->hide();

         ui->pUsageStorage->hide();
         ui->lPercentageUsedStorage->hide();

         ui->pUsageQuota->hide();
         ui->lPercentageUsedQuota->hide();
    }
    else
    {
         ui->lTotalUsedStorage->removeEventFilter(this);
         ui->lTotalUsedStorage->setMouseTracking(false);
         ui->wUsageStorage->installEventFilter(this);
         ui->wUsageStorage->setMouseTracking(true);
         ui->bUpgrade->show();

         ui->pUsageStorage->show();
         ui->lPercentageUsedStorage->show();

         ui->pUsageQuota->show();
         ui->lPercentageUsedQuota->show();
    }
}

void InfoDialog::updateBlockedState()
{
    if (!preferences->logged())
    {
        return;
    }

    if (!waiting)
    {
        if (ui->wBlocked->isVisible())
        {
            ui->lSDKblock->setText(QString::fromUtf8(""));
            ui->wBlocked->setVisible(false);
            ui->wContainerBottom->setFixedHeight(120);
        }
    }
    else
    {
        const char *blockedPath = megaApi->getBlockedPath();
        if (blockedPath)
        {
            QFileInfo fileBlocked (QString::fromUtf8(blockedPath));

            if (ui->sActiveTransfers->currentWidget() != ui->pUpdated)
            {
                ui->wContainerBottom->setFixedHeight(150);
                ui->wBlocked->setVisible(true);
                ui->lSDKblock->setText(tr("Blocked file: %1").arg(QString::fromUtf8("<a href=\"local://#%1\">%2</a>")
                                                                  .arg(fileBlocked.absoluteFilePath())
                                                                  .arg(fileBlocked.fileName())));
            }
            else
            {
                 ui->lSDKblock->setText(QString::fromUtf8(""));
                 ui->wBlocked->setVisible(false);
                 ui->wContainerBottom->setFixedHeight(120);
            }

            ui->lUploadToMegaDesc->setStyleSheet(QString::fromUtf8("font-size: 14px;"));
            ui->lUploadToMegaDesc->setText(tr("Blocked file: %1").arg(QString::fromUtf8("<a href=\"local://#%1\">%2</a>")
                                                           .arg(fileBlocked.absoluteFilePath())
                                                           .arg(fileBlocked.fileName())));
            delete [] blockedPath;
        }
        else if (megaApi->areServersBusy())
        {

            if (ui->sActiveTransfers->currentWidget() != ui->pUpdated)
            {
                ui->wContainerBottom->setFixedHeight(150);
                ui->wBlocked->setVisible(true);
                ui->lSDKblock->setText(tr("The process is taking longer than expected. Please wait..."));
            }
            else
            {
                 ui->lSDKblock->setText(QString::fromUtf8(""));
                 ui->wBlocked->setVisible(false);
                 ui->wContainerBottom->setFixedHeight(120);
            }

            ui->lUploadToMegaDesc->setStyleSheet(QString::fromUtf8("font-size: 14px;"));
            ui->lUploadToMegaDesc->setText(tr("The process is taking longer than expected. Please wait..."));
        }
        else
        {
            if (ui->sActiveTransfers->currentWidget() != ui->pUpdated)
            {
                ui->lSDKblock->setText(QString::fromUtf8(""));
                ui->wBlocked->setVisible(false);
                ui->wContainerBottom->setFixedHeight(120);
            }

            ui->lUploadToMegaDesc->setStyleSheet(QString::fromUtf8("font-size: 14px;"));
            ui->lUploadToMegaDesc->setText(QString::fromUtf8(""));
        }
    }
}

void InfoDialog::updateState()
{
    updateSyncsButton();

    if (!preferences->logged())
    {
        if (gWidget)
        {
            gWidget->resetFocus();
        }
    }

    if (!preferences->logged())
    {
        return;
    }

    if (preferences->getGlobalPaused())
    {
        state = STATE_PAUSED;
        animateStates(waiting || indexing);
    }
    else
    {
        if (waiting)
        {
            if (state != STATE_WAITING)
            {
                state = STATE_WAITING;
                animateStates(true);
            }
        }
        else if (indexing)
        {
            if (state != STATE_INDEXING)
            {
                state = STATE_INDEXING;
                animateStates(true);
            }
        }
        else
        {
            if (state != STATE_UPDATED)
            {
                state = STATE_UPDATED;
                animateStates(false);
            }
        }
    }

    ui->wStatus->setState(state);
}

void InfoDialog::addSync()
{
    addSync(INVALID_HANDLE);
    app->createTrayMenu();
}

void InfoDialog::onAllUploadsFinished()
{
    remainingUploads = megaApi->getNumPendingUploads();
    if (!remainingUploads)
    {
        megaApi->resetTotalUploads();
    }
}

void InfoDialog::onAllDownloadsFinished()
{
    remainingDownloads = megaApi->getNumPendingDownloads();
    if (!remainingDownloads)
    {
        megaApi->resetTotalDownloads();
    }
}

void InfoDialog::onAllTransfersFinished()
{
    if (!remainingDownloads && !remainingUploads)
    {
        if (!overQuotaState && (ui->sActiveTransfers->currentWidget() != ui->pUpdated))
        {
            updateDialogState();
        }

        if ((QDateTime::currentMSecsSinceEpoch() - preferences->lastTransferNotificationTimestamp()) > Preferences::MIN_TRANSFER_NOTIFICATION_INTERVAL_MS)
        {
            app->showNotificationMessage(tr("All transfers have been completed"));
        }
    }
}

void InfoDialog::updateDialogState()
{
    updateState();
    switch (storageState)
    {
        case Preferences::STATE_ALMOST_OVER_STORAGE:
            ui->bOQIcon->setIcon(QIcon(QString::fromLatin1("://images/storage_almost_full.png")));
            ui->bOQIcon->setIconSize(QSize(64,64));
            ui->lOQTitle->setText(tr("You're running out of storage space."));
            ui->lOQDesc->setText(tr("Upgrade to PRO now before your account runs full and your uploads to MEGA stop."));
            ui->sActiveTransfers->setCurrentWidget(ui->pOverquota);
            overlay->setVisible(false);
            ui->wPSA->hidePSA();
            break;
        case Preferences::STATE_OVER_STORAGE:
            ui->bOQIcon->setIcon(QIcon(QString::fromLatin1("://images/storage_full.png")));
            ui->bOQIcon->setIconSize(QSize(64,64));
            ui->lOQTitle->setText(tr("Your MEGA account is full."));
            ui->lOQDesc->setText(tr("All file uploads are currently disabled.")
                                    + QString::fromUtf8("<br>")
                                    + tr("Please upgrade to PRO."));
            ui->sActiveTransfers->setCurrentWidget(ui->pOverquota);
            overlay->setVisible(false);
            ui->wPSA->hidePSA();
            break;
        case Preferences::STATE_BELOW_OVER_STORAGE:
        case Preferences::STATE_OVER_STORAGE_DISMISSED:
        default:
            remainingUploads = megaApi->getNumPendingUploads();
            remainingDownloads = megaApi->getNumPendingDownloads();

            if (remainingUploads || remainingDownloads || (ui->wListTransfers->getModel() && ui->wListTransfers->getModel()->rowCount(QModelIndex())) || ui->wPSA->isPSAready())
            {
                overlay->setVisible(false);
                ui->sActiveTransfers->setCurrentWidget(ui->pTransfers);
                ui->wPSA->showPSA();
            }
            else
            {
                ui->wPSA->hidePSA();
                ui->sActiveTransfers->setCurrentWidget(ui->pUpdated);
                if (!waiting && !indexing)
                {
                    overlay->setVisible(true);
                }
                else
                {
                    overlay->setVisible(false);
                }
            }
            break;
    }
    updateBlockedState();
}

void InfoDialog::on_bSettings_clicked()
{
    emit userActivity();

    QPoint p = ui->bSettings->mapToGlobal(QPoint(ui->bSettings->width() - 2, ui->bSettings->height()));

#ifdef __APPLE__
    QPointer<InfoDialog> iod = this;
#endif

    app->showTrayMenu(&p);

#ifdef __APPLE__
    if (!iod)
    {
        return;
    }

    if (!this->rect().contains(this->mapFromGlobal(QCursor::pos())))
    {
        this->hide();
    }
#endif
}

void InfoDialog::on_bUpgrade_clicked()
{
    QString userAgent = QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(megaApi->getUserAgent())));
    QString url = QString::fromUtf8("pro/uao=%1").arg(userAgent);
    Preferences *preferences = Preferences::instance();
    if (preferences->lastPublicHandleTimestamp() && (QDateTime::currentMSecsSinceEpoch() - preferences->lastPublicHandleTimestamp()) < 86400000)
    {
        mega::MegaHandle aff = preferences->lastPublicHandle();
        if (aff != mega::INVALID_HANDLE)
        {
            char *base64aff = mega::MegaApi::handleToBase64(aff);
            url.append(QString::fromUtf8("/aff=%1/aff_time=%2").arg(QString::fromUtf8(base64aff)).arg(preferences->lastPublicHandleTimestamp() / 1000));
            delete [] base64aff;
        }
    }

    megaApi->getSessionTransferURL(url.toUtf8().constData());
}

void InfoDialog::on_bSyncFolder_clicked()
{
    if (!preferences->logged())
    {
        return;
    }

    MegaNode *rootNode = megaApi->getRootNode();
    if (!rootNode)
    {
        return;
    }

    int num = preferences->getNumSyncedFolders();
    if (num == 0)
    {
        addSync();
    }
    else
    {
        if (syncsMenu)
        {
            for (QAction *a: syncsMenu->actions())
            {
                a->deleteLater();
            }

            syncsMenu->deleteLater();
            syncsMenu.release();
        }

        syncsMenu.reset(new QMenu());

#ifdef __APPLE__
        syncsMenu->setStyleSheet(QString::fromLatin1("QMenu {background: #ffffff; padding-top: 8px; padding-bottom: 8px;}"));
#else
        syncsMenu->setStyleSheet(QString::fromLatin1("QMenu { border: 1px solid #B8B8B8; border-radius: 5px; background: #ffffff; padding-top: 8px; padding-bottom: 8px;}"));
#endif

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
        if (menuSignalMapper)
        {
            menuSignalMapper->deleteLater();
            menuSignalMapper = NULL;
        }

        menuSignalMapper = new QSignalMapper();
        connect(menuSignalMapper, SIGNAL(mapped(QString)), this, SLOT(openFolder(QString)));
#endif
        int activeFolders = 0;
        for (int i = 0; i < num; i++)
        {
            if (!preferences->isFolderActive(i))
            {
                continue;
            }

            activeFolders++;
            MenuItemAction *action = new MenuItemAction(preferences->getSyncName(i), QIcon(QString::fromLatin1("://images/ico_drop_synched_folder.png")),
                                                        QIcon(QString::fromLatin1("://images/ico_drop_synched_folder_over.png")));
            syncsMenu->addAction(action);
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
            connect(action, SIGNAL(triggered()), menuSignalMapper, SLOT(map()));
            menuSignalMapper->setMapping(action, preferences->getLocalFolder(i));
#else
            connect(action, &MenuItemAction::triggered, [this,i]{this->openFolder(preferences->getLocalFolder(i));});
#endif
        }

        if (activeFolders)
        {
            long long firstSyncHandle = INVALID_HANDLE;
            if (num == 1)
            {
                firstSyncHandle = preferences->getMegaFolderHandle(0);
            }

            if (rootNode)
            {
                long long rootHandle = rootNode->getHandle();
                if ((num > 1) || (firstSyncHandle != rootHandle))
                {
                    MenuItemAction *addSyncAction = new MenuItemAction(tr("Add Sync"), QIcon(QString::fromLatin1("://images/ico_add_sync.png")),
                                                                       QIcon(QString::fromLatin1("://images/ico_drop_add_sync_over.png")));
                    connect(addSyncAction, SIGNAL(triggered()), this, SLOT(addSync()), Qt::QueuedConnection);

                    if (activeFolders)
                    {
                        syncsMenu->addSeparator();
                    }
                    syncsMenu->addAction(addSyncAction);
                }
            }
        }
        else
        {
            addSync();
        }

#ifdef __APPLE__
        syncsMenu->exec(this->mapToGlobal(QPoint(20, this->height() - (activeFolders + 1) * 28 - (activeFolders ? 16 : 8))));
        if (!this->rect().contains(this->mapFromGlobal(QCursor::pos())))
        {
            this->hide();
        }
#else
        syncsMenu->popup(ui->bSyncFolder->mapToGlobal(QPoint(-5, (activeFolders ? -21 : -12) - activeFolders * 32)));
#endif
    }
    delete rootNode;
}

void InfoDialog::openFolder(QString path)
{
    QtConcurrent::run(QDesktopServices::openUrl, QUrl::fromLocalFile(path));
}

void InfoDialog::addSync(MegaHandle h)
{
    static QPointer<BindFolderDialog> dialog = NULL;
    if (dialog)
    {
        if (h != mega::INVALID_HANDLE)
        {
            dialog->setMegaFolder(h);
        }

        dialog->activateWindow();
        dialog->raise();
        dialog->setFocus();
        return;
    }

    dialog = new BindFolderDialog(app);
    if (h != mega::INVALID_HANDLE)
    {
        dialog->setMegaFolder(h);
    }

    Platform::execBackgroundWindow(dialog);
    if (!dialog)
    {
        return;
    }

    if (dialog->result() != QDialog::Accepted)
    {
        delete dialog;
        dialog = NULL;
        return;
    }

    QString localFolderPath = QDir::toNativeSeparators(QDir(dialog->getLocalFolder()).canonicalPath());
    MegaHandle handle = dialog->getMegaFolder();
    MegaNode *node = megaApi->getNodeByHandle(handle);
    QString syncName = dialog->getSyncName();
    delete dialog;
    dialog = NULL;
    if (!localFolderPath.length() || !node)
    {
        delete node;
        return;
    }

   const char *nPath = megaApi->getNodePath(node);
   if (!nPath)
   {
       delete node;
       return;
   }

   preferences->addSyncedFolder(localFolderPath, QString::fromUtf8(nPath), handle, syncName);
   delete [] nPath;
   megaApi->syncFolder(localFolderPath.toUtf8().constData(), node);
   delete node;
   updateSyncsButton();
}

#ifdef __APPLE__
void InfoDialog::moveArrow(QPoint p)
{
    arrow->move(p.x()-(arrow->width()/2+1), 2);
    arrow->show();
}
#endif

void InfoDialog::on_bChats_clicked()
{
    QString userAgent = QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(megaApi->getUserAgent())));
    QString url = QString::fromUtf8("").arg(userAgent);
    megaApi->getSessionTransferURL(url.toUtf8().constData());
}

void InfoDialog::onOverlayClicked()
{
    app->uploadActionClicked();
}

void InfoDialog::on_bTransferManager_clicked()
{
    emit userActivity();
    app->transferManagerActionClicked();
}

void InfoDialog::clearUserAttributes()
{
    ui->bAvatar->clearData();
}

bool InfoDialog::updateOverStorageState(int state)
{
    if (storageState != state)
    {
        storageState = state;
        updateDialogState();
        return true;
    }
    return false;
}

QCustomTransfersModel *InfoDialog::stealModel()
{
    QCustomTransfersModel *toret = ui->wListTransfers->getModel();
    ui->wListTransfers->setupTransfers(); // this will create a new empty model
    return toret;
}

void InfoDialog::setPSAannouncement(int id, QString title, QString text, QString urlImage, QString textButton, QString linkButton)
{
    ui->wPSA->setAnnounce(id, title, text, urlImage, textButton, linkButton);
}

void InfoDialog::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *e)
{
    if (transfer->isStreamingTransfer() || transfer->isFolderTransfer())
    {
        return;
    }

    ui->wListTransfers->getModel()->onTransferFinish(api, transfer, e);
}

void InfoDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        if (preferences->logged())
        {
            setUsage();
            state = STATE_STARTING;
            updateDialogState();
            updateSyncsButton();
        }
    }
    QDialog::changeEvent(event);
}

bool InfoDialog::eventFilter(QObject *obj, QEvent *e)
{
#ifdef Q_OS_LINUX
    static bool firstime = true;
    if (getenv("START_MEGASYNC_MINIMIZED") && firstime && (obj == this && e->type() == QEvent::Paint))
    {
        MegaApi::log(MegaApi::LOG_LEVEL_DEBUG, QString::fromUtf8("Minimizing info dialog ...").arg(e->type()).toUtf8().constData());
        showMinimized();
        firstime = false;
    }

    if (doNotActAsPopup)
    {
        if (obj == this && e->type() == QEvent::Close)
        {
            e->ignore(); //This prevents the dialog from closing
            app->exitApplication();
            return true;
        }
    }
    else if (obj == this)
    {
        static bool in = false;
        if (e->type() == QEvent::Enter)
        {
            in = true;
        }
        if (e->type() == QEvent::Leave)
        {
            in = false;
        }
        if (e->type() == QEvent::WindowDeactivate && !in)
        {
            hide();
            return true;
        }
    }
#endif
#ifdef __APPLE__
    if (QSysInfo::MacintoshVersion <= QSysInfo::MV_10_9) //manage spontaneus mouse press events
    {
        if (obj == this && e->type() == QEvent::MouseButtonPress && e->spontaneous())
        {
            return true;
        }
    }
#endif

    return QDialog::eventFilter(obj, e);
}

void InfoDialog::regenerateLayout(InfoDialog* olddialog)
{
    bool logged = preferences->logged();

    if (loggedInMode == logged)
    {
        return;
    }
    loggedInMode = logged;

    QLayout *dialogLayout = layout();
    if (!loggedInMode)
    {
        if (!gWidget)
        {
            gWidget = new GuestWidget();
            connect(gWidget, SIGNAL(forwardAction(int)), this, SLOT(onUserAction(int)));
            if (olddialog)
            {
                auto t = olddialog->gWidget->getTexts();
                gWidget->setTexts(t.first, t.second);
            }
        }
        else
        {
            gWidget->enableListener();
        }

        updateOverStorageState(Preferences::STATE_BELOW_OVER_STORAGE);
        setOverQuotaMode(false);
        on_bDotUsedStorage_clicked();
        ui->wPSA->removeAnnounce();

        ui->bTransferManager->setVisible(false);
        ui->bAvatar->setVisible(false);
        ui->bTransferManager->setVisible(false);
        dialogLayout->removeWidget(ui->wContainerHeader);
        ui->wContainerHeader->setVisible(false);
        dialogLayout->removeWidget(ui->wSeparator);
        ui->wSeparator->setVisible(false);
        dialogLayout->removeWidget(ui->wContainerBottom);
        ui->wContainerBottom->setVisible(false);
        dialogLayout->addWidget(gWidget);
        gWidget->setVisible(true);

        #ifdef __APPLE__
            if (!dummy)
            {
                dummy = new QWidget();
            }

            dummy->resize(1,1);
            dummy->setWindowFlags(Qt::FramelessWindowHint);
            dummy->setAttribute(Qt::WA_NoSystemBackground);
            dummy->setAttribute(Qt::WA_TranslucentBackground);
            dummy->show();

            setMinimumHeight(404);
            setMaximumHeight(404);
        #else
            setMinimumHeight(394);
            setMaximumHeight(394);
        #endif
    }
    else
    {
        gWidget->disableListener();
        gWidget->initialize();

#ifdef __APPLE__
        setMinimumHeight(524);
        setMaximumHeight(524);
#else
        setMinimumHeight(514);
        setMaximumHeight(514);
#endif

        ui->bTransferManager->setVisible(true);
        ui->bAvatar->setVisible(true);
        ui->bTransferManager->setVisible(true);
        dialogLayout->removeWidget(gWidget);
        gWidget->setVisible(false);
        dialogLayout->addWidget(ui->wContainerHeader);
        ui->wContainerHeader->setVisible(true);    
        dialogLayout->addWidget(ui->wSeparator);
        ui->wSeparator->setVisible(true);
        dialogLayout->addWidget(ui->wContainerBottom);
        ui->wContainerBottom->setVisible(true);

        #ifdef __APPLE__
            if (dummy)
            {
                dummy->hide();
                delete dummy;
                dummy = NULL;
            }
        #endif
    }

    app->onGlobalSyncStateChanged(NULL);
}

void InfoDialog::drawAvatar(QString email)
{
    QString avatarsPath = Utilities::getAvatarPath(email);
    QFileInfo avatar(avatarsPath);
    if (avatar.exists())
    {
        ui->bAvatar->setAvatarImage(Utilities::getAvatarPath(email));
    }
    else
    {
        QString color;
        const char* userHandle = megaApi->getMyUserHandle();
        const char* avatarColor = megaApi->getUserAvatarColor(userHandle);
        if (avatarColor)
        {
            color = QString::fromUtf8(avatarColor);
            delete [] avatarColor;
        }

        Preferences *preferences = Preferences::instance();
        QString fullname = (preferences->firstName() + preferences->lastName()).trimmed();
        if (fullname.isEmpty())
        {
            char *email = megaApi->getMyEmail();
            if (email)
            {
                fullname = QString::fromUtf8(email);
                delete [] email;
            }
            else
            {
                fullname = preferences->email();
            }

            if (fullname.isEmpty())
            {
                fullname = QString::fromUtf8(" ");
            }
        }

        ui->bAvatar->setAvatarLetter(fullname.at(0).toUpper(), color);
        delete [] userHandle;
    }
}

void InfoDialog::animateStates(bool opt)
{
    if (opt) //Enable animation for scanning/waiting states
    {        
        ui->lUploadToMega->setIcon(QIcon(QString::fromLatin1("://images/init_scanning.png")));
        ui->lUploadToMega->setIconSize(QSize(352,234));
        ui->lUploadToMegaDesc->setStyleSheet(QString::fromUtf8("font-size: 14px;"));

        if (!opacityEffect)
        {
            opacityEffect = new QGraphicsOpacityEffect();
            ui->lUploadToMega->setGraphicsEffect(opacityEffect);
        }

        if (!animation)
        {
            animation = new QPropertyAnimation(opacityEffect, "opacity");
            animation->setDuration(2000);
            animation->setStartValue(1.0);
            animation->setEndValue(0.5);
            animation->setEasingCurve(QEasingCurve::InOutQuad);
            connect(animation, SIGNAL(finished()), SLOT(onAnimationFinished()));
        }

        if (animation->state() != QAbstractAnimation::Running)
        {
            animation->start();
        }
    }
    else //Disable animation
    {   
        ui->lUploadToMega->setIcon(QIcon(QString::fromLatin1("://images/upload_to_mega.png")));
        ui->lUploadToMega->setIconSize(QSize(352,234));
        ui->lUploadToMegaDesc->setStyleSheet(QString::fromUtf8("font-size: 18px;"));
        ui->lUploadToMegaDesc->setText(tr("Upload to MEGA now"));

        if (animation)
        {
            if (opacityEffect) //Reset opacity
            {
                opacityEffect->setOpacity(1.0);
            }

            if (animation->state() == QAbstractAnimation::Running)
            {
                animation->stop();
            }
        }
    }
}

void InfoDialog::onUserAction(int action)
{
    app->userAction(action);
}

void InfoDialog::on_bDotUsedStorage_clicked()
{
    ui->bDotUsedStorage->setIcon(QIcon(QString::fromLatin1("://images/Nav_Dot_active.png")));
    ui->bDotUsedStorage->setIconSize(QSize(6,6));
    ui->bDotUsedQuota->setIcon(QIcon(QString::fromLatin1("://images/Nav_Dot_inactive.png")));
    ui->bDotUsedQuota->setIconSize(QSize(6,6));

    ui->sUsedData->setCurrentWidget(ui->pStorage);
}

void InfoDialog::on_bDotUsedQuota_clicked()
{
    ui->bDotUsedStorage->setIcon(QIcon(QString::fromLatin1("://images/Nav_Dot_inactive.png")));
    ui->bDotUsedStorage->setIconSize(QSize(6,6));
    ui->bDotUsedQuota->setIcon(QIcon(QString::fromLatin1("://images/Nav_Dot_active.png")));
    ui->bDotUsedQuota->setIconSize(QSize(6,6));

    ui->sUsedData->setCurrentWidget(ui->pQuota);
}

void InfoDialog::on_bDiscard_clicked()
{
    updateOverStorageState(Preferences::STATE_OVER_STORAGE_DISMISSED);
    emit dismissOQ(overQuotaState);
}

void InfoDialog::on_bBuyQuota_clicked()
{
    on_bUpgrade_clicked();
}

void InfoDialog::onAnimationFinished()
{
    if (animation->direction() == QAbstractAnimation::Forward)
    {
        animation->setDirection(QAbstractAnimation::Backward);
        animation->start();
    }
    else
    {
        animation->setDirection(QAbstractAnimation::Forward);
        animation->start();
    }
}

#ifdef __APPLE__
void InfoDialog::paintEvent( QPaintEvent * e)
{
    QDialog::paintEvent(e);
    QPainter p( this );
    p.setCompositionMode( QPainter::CompositionMode_Clear);
    p.fillRect( ui->wArrow->rect(), Qt::transparent );
}
#endif
