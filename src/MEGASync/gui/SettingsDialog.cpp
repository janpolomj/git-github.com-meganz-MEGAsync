#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QRect>
#include <QTranslator>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>

#if QT_VERSION >= 0x050000
#include <QtConcurrent/QtConcurrent>
#endif

#include "MegaApplication.h"
#include "SettingsDialog.h"
#include "QMegaMessageBox.h"
#include "ui_SettingsDialog.h"
#include "control/Utilities.h"
#include "platform/Platform.h"
#include "gui/AddExclusionDialog.h"
#include <assert.h>

#ifdef __APPLE__
    #include "gui/CocoaHelpButton.h"
#endif

#ifdef WIN32
extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#else
#include "gui/PermissionsDialog.h"
#endif

using namespace mega;
using namespace std;

long long calculateCacheSize()
{
    Preferences *preferences = Preferences::instance();
    long long cacheSize = 0;
    for (int i = 0; i < preferences->getNumSyncedFolders(); i++)
    {
        QString syncPath = preferences->getLocalFolder(i);
        if (!syncPath.isEmpty())
        {
            Utilities::getFolderSize(syncPath + QDir::separator() + QString::fromLatin1(MEGA_DEBRIS_FOLDER), &cacheSize);
        }
    }
    return cacheSize;
}

void deleteCache()
{
    ((MegaApplication *)qApp)->cleanLocalCaches(true);
}

long long calculateRemoteCacheSize(MegaApi *megaApi)
{
    MegaNode *n = megaApi->getNodeByPath("//bin/SyncDebris");
    long long toret = megaApi->getSize(n);
    delete n;
    return toret;
}

void deleteRemoteCache(MegaApi *megaApi)
{
    MegaNode *n = megaApi->getNodeByPath("//bin/SyncDebris");
    megaApi->remove(n);
    delete n;
}

SettingsDialog::SettingsDialog(MegaApplication *app, bool proxyOnly, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    this->app = app;
    this->megaApi = app->getMegaApi();
    this->preferences = Preferences::instance();
    syncsChanged = false;
    excludedNamesChanged = false;
    sizeLimitsChanged = false;
    cleanerLimitsChanged = false;
    fileVersioningChanged = false;
#ifndef WIN32
    filePermissions = 0;
    folderPermissions = 0;
    permissionsChanged = false;
#endif
    this->proxyOnly = proxyOnly;
    this->proxyTestProgressDialog = NULL;
    shouldClose = false;
    modifyingSettings = 0;
    accountDetailsDialog = NULL;
    cacheSize = -1;
    remoteCacheSize = -1;
    fileVersionsSize = preferences->logged() ? preferences->versionsStorage() : 0;

    reloadUIpage = false;
    hasUpperLimit = false;
    hasLowerLimit = false;
    upperLimit = 0;
    lowerLimit = 0;
    upperLimitUnit = Preferences::MEGA_BYTE_UNIT;
    lowerLimitUnit = Preferences::MEGA_BYTE_UNIT;
    debugCounter = 0;
    hasDaysLimit = false;
    daysLimit = 0;

    ui->eProxyPort->setValidator(new QIntValidator(0, 65535, this));
    ui->eUploadLimit->setValidator(new QIntValidator(0, 1000000000, this));
    ui->eDownloadLimit->setValidator(new QIntValidator(0, 1000000000, this));
    ui->eMaxDownloadConnections->setRange(1, 6);
    ui->eMaxUploadConnections->setRange(1, 6);

    ui->tNoHttp->viewport()->setCursor(Qt::ArrowCursor);
    downloadButtonGroup.addButton(ui->rDownloadLimit);
    downloadButtonGroup.addButton(ui->rDownloadNoLimit);
    uploadButtonGroup.addButton(ui->rUploadLimit);
    uploadButtonGroup.addButton(ui->rUploadNoLimit);
    uploadButtonGroup.addButton(ui->rUploadAutoLimit);

    ui->bAccount->setChecked(true);
    ui->wStack->setCurrentWidget(ui->pAccount);

    connect(ui->rNoProxy, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->rProxyAuto, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->rProxyManual, SIGNAL(clicked()), this, SLOT(proxyStateChanged()));
    connect(ui->eProxyUsername, SIGNAL(textChanged(QString)), this, SLOT(proxyStateChanged()));
    connect(ui->eProxyPassword, SIGNAL(textChanged(QString)), this, SLOT(proxyStateChanged()));
    connect(ui->eProxyServer, SIGNAL(textChanged(QString)), this, SLOT(proxyStateChanged()));
    connect(ui->eProxyPort, SIGNAL(textChanged(QString)), this, SLOT(proxyStateChanged()));
    connect(ui->cProxyType, SIGNAL(currentIndexChanged(int)), this, SLOT(proxyStateChanged()));
    connect(ui->cProxyRequiresPassword, SIGNAL(clicked()), this, SLOT(proxyStateChanged()));

    connect(ui->cStartOnStartup, SIGNAL(stateChanged(int)), this, SLOT(stateChanged()));
    connect(ui->cShowNotifications, SIGNAL(stateChanged(int)), this, SLOT(stateChanged()));
    connect(ui->cOverlayIcons, SIGNAL(stateChanged(int)), this, SLOT(stateChanged()));
    connect(ui->cAutoUpdate, SIGNAL(stateChanged(int)), this, SLOT(stateChanged()));
    connect(ui->cLanguage, SIGNAL(currentIndexChanged(int)), this, SLOT(stateChanged()));
    connect(ui->cDisableFileVersioning, SIGNAL(clicked(bool)), this, SLOT(fileVersioningStateChanged()));

    connect(ui->rDownloadNoLimit, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->rDownloadLimit, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->eDownloadLimit, SIGNAL(textChanged(QString)), this, SLOT(stateChanged()));
    connect(ui->rUploadAutoLimit, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->rUploadNoLimit, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->rUploadLimit, SIGNAL(clicked()), this, SLOT(stateChanged()));
    connect(ui->eUploadLimit, SIGNAL(textChanged(QString)), this, SLOT(stateChanged()));
    connect(ui->eMaxDownloadConnections, SIGNAL(valueChanged(int)), this, SLOT(stateChanged()));
    connect(ui->eMaxUploadConnections, SIGNAL(valueChanged(int)), this, SLOT(stateChanged()));
    connect(ui->cbUseHttps, SIGNAL(clicked()), this, SLOT(stateChanged()));

#ifndef WIN32    
    #ifndef __APPLE__
    ui->rProxyAuto->hide();
    ui->cDisableIcons->hide();
    ui->cAutoUpdate->hide();
    ui->bUpdate->hide();
    #endif
#else
    connect(ui->cDisableIcons, SIGNAL(clicked()), this, SLOT(stateChanged()));
    ui->cDisableIcons->hide();

    typedef LONG MEGANTSTATUS;
    typedef struct _MEGAOSVERSIONINFOW {
        DWORD dwOSVersionInfoSize;
        DWORD dwMajorVersion;
        DWORD dwMinorVersion;
        DWORD dwBuildNumber;
        DWORD dwPlatformId;
        WCHAR  szCSDVersion[ 128 ];     // Maintenance string for PSS usage
    } MEGARTL_OSVERSIONINFOW, *PMEGARTL_OSVERSIONINFOW;

    typedef MEGANTSTATUS (WINAPI* RtlGetVersionPtr)(PMEGARTL_OSVERSIONINFOW);
    MEGARTL_OSVERSIONINFOW version = { 0 };
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod)
    {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion)
        {
            RtlGetVersion(&version);
            if (version.dwMajorVersion >= 10)
            {
                ui->cDisableIcons->show();
            }
        }
    }
#endif
    ui->cProxyType->addItem(QString::fromUtf8("SOCKS5H"));

#ifdef __APPLE__
    this->setWindowTitle(tr("Preferences - MEGAsync"));
    ui->cStartOnStartup->setText(tr("Open at login"));
    if (QSysInfo::MacintoshVersion <= QSysInfo::MV_10_9) //FinderSync API support from 10.10+
    {
        ui->cOverlayIcons->hide();
    }

    CocoaHelpButton *helpButton = new CocoaHelpButton(this);
    ui->layoutBottom->insertWidget(0, helpButton);
    connect(helpButton, SIGNAL(clicked()), this, SLOT(on_bHelp_clicked()));
#endif

    if (!proxyOnly && preferences->logged())
    {
        connect(&cacheSizeWatcher, SIGNAL(finished()), this, SLOT(onLocalCacheSizeAvailable()));
        QFuture<long long> futureCacheSize = QtConcurrent::run(calculateCacheSize);
        cacheSizeWatcher.setFuture(futureCacheSize);

        connect(&remoteCacheSizeWatcher, SIGNAL(finished()), this, SLOT(onRemoteCacheSizeAvailable()));
        QFuture<long long> futureRemoteCacheSize = QtConcurrent::run(calculateRemoteCacheSize,megaApi);
        remoteCacheSizeWatcher.setFuture(futureRemoteCacheSize);
    }

#ifdef __APPLE__
    ui->bOk->hide();
    ui->bCancel->hide();

    const qreal ratio = qApp->testAttribute(Qt::AA_UseHighDpiPixmaps) ? devicePixelRatio() : 1.0;
    if (ratio < 2)
    {
        ui->wTabHeader->setStyleSheet(QString::fromUtf8("#wTabHeader { border-image: url(\":/images/menu_header.png\"); }"));

        ui->bAccount->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
        ui->bBandwidth->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
        ui->bProxies->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
        ui->bSyncs->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
        ui->bAdvanced->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
    }
    else
    {
        ui->wTabHeader->setStyleSheet(QString::fromUtf8("#wTabHeader { border-image: url(\":/images/menu_header@2x.png\"); }"));

        ui->bAccount->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected@2x.png\"); }"));
        ui->bBandwidth->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected@2x.png\"); }"));
        ui->bProxies->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected@2x.png\"); }"));
        ui->bSyncs->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected@2x.png\"); }"));
        ui->bAdvanced->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected@2x.png\"); }"));
    }

#else

    ui->wTabHeader->setStyleSheet(QString::fromUtf8("#wTabHeader { border-image: url(\":/images/menu_header.png\"); }"));
    ui->bAccount->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
    ui->bBandwidth->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
    ui->bProxies->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
    ui->bSyncs->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
    ui->bAdvanced->setStyleSheet(QString::fromUtf8("QToolButton:checked { border-image: url(\":/images/menu_selected.png\"); }"));
#endif

    ui->bLocalCleaner->setText(ui->bLocalCleaner->text().arg(QString::fromLatin1(MEGA_DEBRIS_FOLDER)));

    ui->gCache->setVisible(false);
    ui->lFileVersionsSize->setVisible(false);
    ui->bClearFileVersions->setVisible(false);
    setProxyOnly(proxyOnly);
    ui->bOk->setDefault(true);

#ifdef __APPLE__
    minHeightAnimation = new QPropertyAnimation();
    maxHeightAnimation = new QPropertyAnimation();
    animationGroup = new QParallelAnimationGroup();
    animationGroup->addAnimation(minHeightAnimation);
    animationGroup->addAnimation(maxHeightAnimation);
    connect(animationGroup, SIGNAL(finished()), this, SLOT(onAnimationFinished()));

    ui->pAdvanced->hide();
    ui->pBandwidth->hide();
    ui->pSyncs->hide();

    if (!proxyOnly)
    {
        ui->pProxies->hide();
        setMinimumHeight(485);
        setMaximumHeight(485);
        ui->bApply->hide();
    }
#endif

    ui->lOQWarning->setText(QString::fromUtf8(""));
    ui->wOQError->hide();

    highDpiResize.init(this);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setProxyOnly(bool proxyOnly)
{
    this->proxyOnly = proxyOnly;
    loadSettings();
    if (proxyOnly)
    {
        ui->bAccount->setEnabled(false);
        ui->bAccount->setChecked(false);
        ui->bAdvanced->setEnabled(false);
        ui->bAdvanced->setChecked(false);
        ui->bSyncs->setEnabled(false);
        ui->bSyncs->setChecked(false);
        ui->bBandwidth->setEnabled(false);
        ui->bBandwidth->setChecked(false);
        ui->bProxies->setChecked(true);
        ui->wStack->setCurrentWidget(ui->pProxies);
        ui->pProxies->show();

#ifdef __APPLE__
        setMinimumHeight(435);
        setMaximumHeight(435);
        ui->bApply->show();
#endif
    }
    else
    {
        ui->bAccount->setEnabled(true);
        ui->bAdvanced->setEnabled(true);
        ui->bSyncs->setEnabled(true);
        ui->bBandwidth->setEnabled(true);
    }
}

void SettingsDialog::setOverQuotaMode(bool mode)
{
    if (mode)
    {
        QString userAgent = QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(megaApi->getUserAgent())));
        QString url = QString::fromUtf8("pro/uao=%1").arg(userAgent);
        Preferences *preferences = Preferences::instance();
        if (preferences->lastPublicHandleTimestamp() && (QDateTime::currentMSecsSinceEpoch() - preferences->lastPublicHandleTimestamp()) < 86400000)
        {
            MegaHandle aff = preferences->lastPublicHandle();
            if (aff != INVALID_HANDLE)
            {
                char *base64aff = MegaApi::handleToBase64(aff);
                url.append(QString::fromUtf8("/aff=%1/aff_time=%2").arg(QString::fromUtf8(base64aff)).arg(preferences->lastPublicHandleTimestamp() / 1000));
                delete [] base64aff;
            }
        }

        ui->lOQWarning->setText(tr("Your MEGA account is full. All uploads are disabled, which may affect your synced folders. [A]Buy more space[/A]")
                                        .replace(QString::fromUtf8("[A]"), QString::fromUtf8("<a href=\"mega://#%1\"><span style=\"color:#d90007; text-decoration:none;\">").arg(url))
                                        .replace(QString::fromUtf8("[/A]"), QString::fromUtf8("</span></a>")));
        ui->wOQError->show();
    }
    else
    {
        ui->lOQWarning->setText(QString::fromUtf8(""));
        ui->wOQError->hide();
    }

    return;
}

void SettingsDialog::stateChanged()
{
    if (modifyingSettings)
    {
        return;
    }

#ifndef __APPLE__
    ui->bApply->setEnabled(true);
#else
    this->on_bApply_clicked();
#endif
}

void SettingsDialog::fileVersioningStateChanged()
{
    QPointer<SettingsDialog> dialog = QPointer<SettingsDialog>(this);
    if (ui->cDisableFileVersioning->isChecked() && (QMegaMessageBox::warning(NULL,
                             QString::fromUtf8("MEGAsync"),
                             tr("Disabling file versioning will prevent the creation and storage of new file versions. Do you want to continue?"),
                             Utilities::getDevicePixelRatio(), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes
            || !dialog))
    {
        if (dialog)
        {
            ui->cDisableFileVersioning->setChecked(false);
        }
        return;
    }

    fileVersioningChanged = true;
    stateChanged();
}

void SettingsDialog::syncStateChanged(int state)
{
    if (state)
    {
        Platform::prepareForSync();
        QPointer<QCheckBox> c = ((QCheckBox *)QObject::sender());
        for (int j = 0; j < ui->tSyncs->rowCount(); j++)
        {
            if (ui->tSyncs->cellWidget(j, 2) == c)
            {
                QString newLocalPath = ui->tSyncs->item(j, 0)->text();
                QFileInfo fi(newLocalPath);
                if (!fi.exists() || !fi.isDir())
                {
                    c->setCheckState(Qt::Unchecked);
                    QMessageBox::critical(NULL, tr("Error"),
                       tr("This sync can't be enabled because the local folder doesn't exist"));
                    return;
                }

                QString newMegaPath = ui->tSyncs->item(j, 1)->text();
                MegaNode *n = megaApi->getNodeByPath(newMegaPath.toUtf8().constData());
                if (!n)
                {
                    c->setCheckState(Qt::Unchecked);
                    QMessageBox::critical(NULL, tr("Error"),
                       tr("This sync can't be enabled because the remote folder doesn't exist"));
                    return;
                }
                delete n;
                break;
            }
        }
    }

    syncsChanged = true;
    stateChanged();
}

void SettingsDialog::proxyStateChanged()
{
    if (modifyingSettings)
    {
        return;
    }

    ui->bApply->setEnabled(true);
}

void SettingsDialog::onLocalCacheSizeAvailable()
{
    cacheSize = cacheSizeWatcher.result();
    onCacheSizeAvailable();
}

void SettingsDialog::onRemoteCacheSizeAvailable()
{
    remoteCacheSize = remoteCacheSizeWatcher.result();
    onCacheSizeAvailable();
}

void SettingsDialog::storageChanged()
{
    onCacheSizeAvailable();
}

void SettingsDialog::onCacheSizeAvailable()
{
    if (cacheSize != -1 && remoteCacheSize != -1)
    {
        if (!cacheSize && !remoteCacheSize && !fileVersionsSize)
        {
            return;
        }

        if (cacheSize)
        {
            ui->lCacheSize->setText(QString::fromUtf8(MEGA_DEBRIS_FOLDER) + QString::fromUtf8(": %1").arg(Utilities::getSizeString(cacheSize)));
            ui->gCache->setVisible(true);
        }
        else
        {
            //Hide and remove from layout to avoid  uneeded space
            ui->lCacheSize->hide();
            ui->bClearCache->hide();

            // Move remote SyncDebris widget to left side
            ui->gCache->layout()->removeWidget(ui->wLocalCache);
            ui->wRemoteCache->layout()->removeItem(ui->rSpacer);
#ifndef __APPLE__
            ui->lRemoteCacheSize->setMargin(2);
#endif
            ((QBoxLayout *)ui->gCache->layout())->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Fixed));
        }

        if (remoteCacheSize)
        {
            ui->lRemoteCacheSize->setText(QString::fromUtf8("SyncDebris: %1").arg(Utilities::getSizeString(remoteCacheSize)));
            ui->gCache->setVisible(true);
        }
        else
        {
            //Hide and remove from layout to avoid  uneeded space
            ui->lRemoteCacheSize->hide();
            ui->bClearRemoteCache->hide();
        }

        fileVersionsSize = preferences->logged() ? preferences->versionsStorage() : 0;
        if (fileVersionsSize)
        {
            ui->lFileVersionsSize->setText(tr("File versions: %1").arg(Utilities::getSizeString(fileVersionsSize)));
            ui->lFileVersionsSize->setVisible(true);
            ui->bClearFileVersions->setVisible(true);
        }
        else
        {
            //Hide and remove from layout to avoid  uneeded space
            ui->lFileVersionsSize->hide();
            ui->bClearFileVersions->hide();
        }

    #ifdef __APPLE__
        if (ui->wStack->currentWidget() == ui->pAdvanced)
        {
            minHeightAnimation->setTargetObject(this);
            maxHeightAnimation->setTargetObject(this);
            minHeightAnimation->setPropertyName("minimumHeight");
            maxHeightAnimation->setPropertyName("maximumHeight");
            minHeightAnimation->setStartValue(minimumHeight());
            maxHeightAnimation->setStartValue(maximumHeight());
            minHeightAnimation->setEndValue(572);
            maxHeightAnimation->setEndValue(572);
            minHeightAnimation->setDuration(150);
            maxHeightAnimation->setDuration(150);
            animationGroup->start();
        }
    #endif
    }
}
void SettingsDialog::on_bAccount_clicked()
{
    emit userActivity();

    if (ui->wStack->currentWidget() == ui->pAccount && !reloadUIpage)
    {
        ui->bAccount->setChecked(true);
        return;
    }

    reloadUIpage = false;

#ifdef __APPLE__
    ui->bApply->hide();
#endif

    ui->bAccount->setChecked(true);
    ui->bSyncs->setChecked(false);
    ui->bBandwidth->setChecked(false);
    ui->bAdvanced->setChecked(false);
    ui->bProxies->setChecked(false);
    ui->wStack->setCurrentWidget(ui->pAccount);
    ui->bOk->setFocus();

#ifdef __APPLE__
    ui->pAccount->hide();
    ui->pAdvanced->hide();
    ui->pBandwidth->hide();
    ui->pProxies->hide();
    ui->pSyncs->hide();

    minHeightAnimation->setTargetObject(this);
    maxHeightAnimation->setTargetObject(this);
    minHeightAnimation->setPropertyName("minimumHeight");
    maxHeightAnimation->setPropertyName("maximumHeight");
    minHeightAnimation->setStartValue(minimumHeight());
    maxHeightAnimation->setStartValue(maximumHeight());
    if (preferences->accountType() == Preferences::ACCOUNT_TYPE_BUSINESS)
    {
        minHeightAnimation->setEndValue(465);
        maxHeightAnimation->setEndValue(465);

        ui->gStorageSpace->setMinimumHeight(83);
    }
    else
    {
        minHeightAnimation->setEndValue(485);
        maxHeightAnimation->setEndValue(485);

        ui->gStorageSpace->setMinimumHeight(103);
    }
    minHeightAnimation->setDuration(150);
    maxHeightAnimation->setDuration(150);
    animationGroup->start();
#endif
}

void SettingsDialog::on_bSyncs_clicked()
{
    emit userActivity();

    if (ui->wStack->currentWidget() == ui->pSyncs)
    {
        ui->bSyncs->setChecked(true);
        return;
    }

#ifdef __APPLE__
    ui->bApply->hide();
#endif

    ui->bAccount->setChecked(false);
    ui->bSyncs->setChecked(true);
    ui->bBandwidth->setChecked(false);
    ui->bAdvanced->setChecked(false);
    ui->bProxies->setChecked(false);
    ui->wStack->setCurrentWidget(ui->pSyncs);
    ui->tSyncs->horizontalHeader()->setVisible( true );
    ui->bOk->setFocus();

#ifdef __APPLE__
    ui->pAccount->hide();
    ui->pAdvanced->hide();
    ui->pBandwidth->hide();
    ui->pProxies->hide();
    ui->pSyncs->hide();

    minHeightAnimation->setTargetObject(this);
    maxHeightAnimation->setTargetObject(this);
    minHeightAnimation->setPropertyName("minimumHeight");
    maxHeightAnimation->setPropertyName("maximumHeight");
    minHeightAnimation->setStartValue(minimumHeight());
    maxHeightAnimation->setStartValue(maximumHeight());

    minHeightAnimation->setEndValue(420);
    maxHeightAnimation->setEndValue(420);
    minHeightAnimation->setDuration(150);
    maxHeightAnimation->setDuration(150);
    animationGroup->start();
#endif
}

void SettingsDialog::on_bBandwidth_clicked()
{
    emit userActivity();

    if (ui->wStack->currentWidget() == ui->pBandwidth)
    {
        ui->bBandwidth->setChecked(true);
        return;
    }

#ifdef __APPLE__
    ui->bApply->hide();
#endif

    ui->bAccount->setChecked(false);
    ui->bSyncs->setChecked(false);
    ui->bBandwidth->setChecked(true);
    ui->bAdvanced->setChecked(false);
    ui->bProxies->setChecked(false);
    ui->wStack->setCurrentWidget(ui->pBandwidth);
    ui->bOk->setFocus();

#ifdef __APPLE__
    ui->pAccount->hide();
    ui->pAdvanced->hide();
    ui->pBandwidth->hide();
    ui->pProxies->hide();
    ui->pSyncs->hide();

    int bwHeight;
    ui->gBandwidthQuota->show();
    ui->bSeparatorBandwidth->show();

    minHeightAnimation->setTargetObject(this);
    maxHeightAnimation->setTargetObject(this);
    minHeightAnimation->setPropertyName("minimumHeight");
    maxHeightAnimation->setPropertyName("maximumHeight");
    minHeightAnimation->setStartValue(minimumHeight());
    maxHeightAnimation->setStartValue(maximumHeight());
    if (preferences->accountType() == Preferences::ACCOUNT_TYPE_BUSINESS)
    {
        minHeightAnimation->setEndValue(520);
        maxHeightAnimation->setEndValue(520);

        ui->gBandwidthQuota->setMinimumHeight(59);
    }
    else
    {
        minHeightAnimation->setEndValue(540);
        maxHeightAnimation->setEndValue(540);

        ui->gBandwidthQuota->setMinimumHeight(79);
    }
    minHeightAnimation->setDuration(150);
    maxHeightAnimation->setDuration(150);
    animationGroup->start();
#endif
}

void SettingsDialog::on_bAdvanced_clicked()
{
    emit userActivity();

    if (ui->wStack->currentWidget() == ui->pAdvanced)
    {
        ui->bAdvanced->setChecked(true);
        return;
    }

#ifdef __APPLE__
    ui->bApply->hide();
#endif

    ui->bAccount->setChecked(false);
    ui->bSyncs->setChecked(false);
    ui->bBandwidth->setChecked(false);
    ui->bAdvanced->setChecked(true);
    ui->bProxies->setChecked(false);
    ui->wStack->setCurrentWidget(ui->pAdvanced);
    ui->bOk->setFocus();

#ifdef __APPLE__
    ui->pAccount->hide();
    ui->pAdvanced->hide();
    ui->pBandwidth->hide();
    ui->pProxies->hide();
    ui->pSyncs->hide();

    minHeightAnimation->setTargetObject(this);
    maxHeightAnimation->setTargetObject(this);
    minHeightAnimation->setPropertyName("minimumHeight");
    maxHeightAnimation->setPropertyName("maximumHeight");
    minHeightAnimation->setStartValue(minimumHeight());
    maxHeightAnimation->setStartValue(maximumHeight());

    onCacheSizeAvailable();

    if (!cacheSize && !remoteCacheSize)
    {
        minHeightAnimation->setEndValue(595);
        maxHeightAnimation->setEndValue(595);
    }
    else
    {
        minHeightAnimation->setEndValue(640);
        maxHeightAnimation->setEndValue(640);
    }

    minHeightAnimation->setDuration(150);
    maxHeightAnimation->setDuration(150);
    animationGroup->start();
#endif
}


void SettingsDialog::on_bProxies_clicked()
{
    emit userActivity();

    if (ui->wStack->currentWidget() == ui->pProxies)
    {
        ui->bProxies->setChecked(true);
        return;
    }

#ifdef __APPLE__
    ui->bApply->show();
#endif

    ui->bAccount->setChecked(false);
    ui->bSyncs->setChecked(false);
    ui->bBandwidth->setChecked(false);
    ui->bAdvanced->setChecked(false);
    ui->bProxies->setChecked(true);
    ui->wStack->setCurrentWidget(ui->pProxies);
    ui->bOk->setFocus();

#ifdef __APPLE__
    ui->pAccount->hide();
    ui->pAdvanced->hide();
    ui->pBandwidth->hide();
    ui->pProxies->hide();
    ui->pSyncs->hide();

    minHeightAnimation->setTargetObject(this);
    maxHeightAnimation->setTargetObject(this);
    minHeightAnimation->setPropertyName("minimumHeight");
    maxHeightAnimation->setPropertyName("maximumHeight");
    minHeightAnimation->setStartValue(minimumHeight());
    maxHeightAnimation->setStartValue(maximumHeight());
    minHeightAnimation->setEndValue(435);
    maxHeightAnimation->setEndValue(435);
    minHeightAnimation->setDuration(150);
    maxHeightAnimation->setDuration(150);
    animationGroup->start();
#endif
}


void SettingsDialog::on_bCancel_clicked()
{
    this->close();
}

void SettingsDialog::on_bOk_clicked()
{
    bool saved = 1;
    if (ui->bApply->isEnabled())
    {
        saved = saveSettings();
    }

    if (saved == 1)
    {
        this->close();
    }
    else if (!saved)
    {
        shouldClose = true;
    }
}

void SettingsDialog::on_bHelp_clicked()
{
    QString helpUrl = Preferences::BASE_URL + QString::fromLatin1("/help/client/megasync");
    QtConcurrent::run(QDesktopServices::openUrl, QUrl(helpUrl));
}

#ifndef __APPLE__
void SettingsDialog::on_bHelpIco_clicked()
{
    on_bHelp_clicked();
}
#endif

void SettingsDialog::on_rProxyManual_clicked()
{
    ui->cProxyType->setEnabled(true);
    ui->eProxyServer->setEnabled(true);
    ui->eProxyPort->setEnabled(true);
    ui->cProxyRequiresPassword->setEnabled(true);
    if (ui->cProxyRequiresPassword->isChecked())
    {
        ui->eProxyUsername->setEnabled(true);
        ui->eProxyPassword->setEnabled(true);
    }
    else
    {
        ui->eProxyUsername->setEnabled(false);
        ui->eProxyPassword->setEnabled(false);
    }
}

void SettingsDialog::on_rProxyAuto_clicked()
{
    ui->cProxyType->setEnabled(false);
    ui->eProxyServer->setEnabled(false);
    ui->eProxyPort->setEnabled(false);
    ui->eProxyUsername->setEnabled(false);
    ui->eProxyPassword->setEnabled(false);
    ui->cProxyRequiresPassword->setEnabled(false);
}

void SettingsDialog::on_rNoProxy_clicked()
{
    ui->cProxyType->setEnabled(false);
    ui->eProxyServer->setEnabled(false);
    ui->eProxyPort->setEnabled(false);
    ui->eProxyUsername->setEnabled(false);
    ui->eProxyPassword->setEnabled(false);
    ui->cProxyRequiresPassword->setEnabled(false);
}

void SettingsDialog::on_bUpgrade_clicked()
{
    QString userAgent = QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(megaApi->getUserAgent())));
    QString url = QString::fromUtf8("pro/uao=%1").arg(userAgent);
    Preferences *preferences = Preferences::instance();
    if (preferences->lastPublicHandleTimestamp() && (QDateTime::currentMSecsSinceEpoch() - preferences->lastPublicHandleTimestamp()) < 86400000)
    {
        MegaHandle aff = preferences->lastPublicHandle();
        if (aff != INVALID_HANDLE)
        {
            char *base64aff = MegaApi::handleToBase64(aff);
            url.append(QString::fromUtf8("/aff=%1/aff_time=%2").arg(QString::fromUtf8(base64aff)).arg(preferences->lastPublicHandleTimestamp() / 1000));
            delete [] base64aff;
        }
    }

    megaApi->getSessionTransferURL(url.toUtf8().constData());
}

void SettingsDialog::on_bUpgradeBandwidth_clicked()
{
    QString userAgent = QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(megaApi->getUserAgent())));
    QString url = QString::fromUtf8("pro/uao=%1").arg(userAgent);
    Preferences *preferences = Preferences::instance();
    if (preferences->lastPublicHandleTimestamp() && (QDateTime::currentMSecsSinceEpoch() - preferences->lastPublicHandleTimestamp()) < 86400000)
    {
        MegaHandle aff = preferences->lastPublicHandle();
        if (aff != INVALID_HANDLE)
        {
            char *base64aff = MegaApi::handleToBase64(aff);
            url.append(QString::fromUtf8("/aff=%1/aff_time=%2").arg(QString::fromUtf8(base64aff)).arg(preferences->lastPublicHandleTimestamp() / 1000));
            delete [] base64aff;
        }
    }

    megaApi->getSessionTransferURL(url.toUtf8().constData());
}

void SettingsDialog::on_rUploadAutoLimit_clicked()
{
    ui->eUploadLimit->setEnabled(false);
}

void SettingsDialog::on_rUploadNoLimit_clicked()
{
    ui->eUploadLimit->setEnabled(false);
}

void SettingsDialog::on_rUploadLimit_clicked()
{
    ui->eUploadLimit->setEnabled(true);
}

void SettingsDialog::on_rDownloadNoLimit_clicked()
{
    ui->eDownloadLimit->setEnabled(false);
}

void SettingsDialog::on_rDownloadLimit_clicked()
{
    ui->eDownloadLimit->setEnabled(true);
}

void SettingsDialog::on_cProxyRequiresPassword_clicked()
{
    if (ui->cProxyRequiresPassword->isChecked())
    {
        ui->eProxyUsername->setEnabled(true);
        ui->eProxyPassword->setEnabled(true);
    }
    else
    {
        ui->eProxyUsername->setEnabled(false);
        ui->eProxyPassword->setEnabled(false);
    }
}

void SettingsDialog::loadSettings()
{
    modifyingSettings++;

    if (!proxyOnly)
    {
        //General
        ui->cShowNotifications->setChecked(preferences->showNotifications());

        if (!preferences->canUpdate(MegaApplication::applicationFilePath()))
        {
            ui->bUpdate->setEnabled(false);
            ui->cAutoUpdate->setEnabled(false);
            ui->cAutoUpdate->setChecked(false);
        }
        else
        {
            ui->bUpdate->setEnabled(true);
            ui->cAutoUpdate->setEnabled(true);
            ui->cAutoUpdate->setChecked(preferences->updateAutomatically());
        }

        // if checked: make sure both sources are true
        ui->cStartOnStartup->setChecked(preferences->startOnStartup() && Platform::isStartOnStartupActive());

        //Language
        ui->cLanguage->clear();
        languageCodes.clear();
        QString fullPrefix = Preferences::TRANSLATION_FOLDER+Preferences::TRANSLATION_PREFIX;
        QDirIterator it(Preferences::TRANSLATION_FOLDER);
        QStringList languages;
        int currentIndex = -1;
        QString currentLanguage = preferences->language();
        while (it.hasNext())
        {
            QString file = it.next();
            if (file.startsWith(fullPrefix))
            {
                int extensionIndex = file.lastIndexOf(QString::fromLatin1("."));
                if ((extensionIndex - fullPrefix.size()) <= 0)
                {
                    continue;
                }

                QString languageCode = file.mid(fullPrefix.size(), extensionIndex-fullPrefix.size());
                QString languageString = Utilities::languageCodeToString(languageCode);
                if (!languageString.isEmpty())
                {
                    int i = 0;
                    while (i < languages.size() && (languageString > languages[i]))
                    {
                        i++;
                    }
                    languages.insert(i, languageString);
                    languageCodes.insert(i, languageCode);
                }
            }
        }

        for (int i = languageCodes.size() - 1; i >= 0; i--)
        {
            if (currentLanguage.startsWith(languageCodes[i]))
            {
                currentIndex = i;
                break;
            }
        }

        if (currentIndex == -1)
        {
            currentIndex = languageCodes.indexOf(QString::fromLatin1("en"));
        }

        ui->cLanguage->addItems(languages);
        ui->cLanguage->setCurrentIndex(currentIndex);

        int width = ui->bBandwidth->width();
        QFont f = ui->bBandwidth->font();
        QFontMetrics fm = QFontMetrics(f);
#if QT_VERSION < QT_VERSION_CHECK(5,11,0)
        int neededWidth = fm.width(tr("Bandwidth"));
#else
        int neededWidth = fm.horizontalAdvance(tr("Bandwidth"));
#endif
        if (width < neededWidth)
        {
            ui->bBandwidth->setText(tr("Transfers"));
        }

        if (ui->lUploadAutoLimit->text().trimmed().at(0)!=QChar::fromLatin1('('))
        {
            ui->lUploadAutoLimit->setText(QString::fromLatin1("(%1)").arg(ui->lUploadAutoLimit->text().trimmed()));
        }

        // Disable Upgrade buttons for business accounts
        if (preferences->accountType() == Preferences::ACCOUNT_TYPE_BUSINESS)
        {
            ui->bUpgrade->hide();
            ui->bUpgradeBandwidth->hide();
        }
        else
        {
            ui->bUpgrade->show();
            ui->bUpgradeBandwidth->show();
        }

        //Account
        char *email = megaApi->getMyEmail();
        if (email)
        {
            ui->lEmail->setText(QString::fromUtf8(email));
            delete [] email;
        }
        else
        {
            ui->lEmail->setText(preferences->email());
        }
        refreshAccountDetails();

        QIcon icon;
        int accType = preferences->accountType();
        switch(accType)
        {
            case Preferences::ACCOUNT_TYPE_FREE:
                icon.addFile(QString::fromUtf8(":/images/Free.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(tr("FREE"));
                break;
            case Preferences::ACCOUNT_TYPE_PROI:
                icon.addFile(QString::fromUtf8(":/images/Pro_I.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(tr("PRO I"));
                break;
            case Preferences::ACCOUNT_TYPE_PROII:
                icon.addFile(QString::fromUtf8(":/images/Pro_II.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(tr("PRO II"));
                break;
            case Preferences::ACCOUNT_TYPE_PROIII:
                icon.addFile(QString::fromUtf8(":/images/Pro_III.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(tr("PRO III"));
                break;
            case Preferences::ACCOUNT_TYPE_LITE:
                icon.addFile(QString::fromUtf8(":/images/Lite.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(tr("PRO Lite"));
                break;
            case Preferences::ACCOUNT_TYPE_BUSINESS:
                icon.addFile(QString::fromUtf8(":/images/business.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(QString::fromUtf8("BUSINESS"));
                break;
            default:
                icon.addFile(QString::fromUtf8(":/images/Pro_I.png"), QSize(), QIcon::Normal, QIcon::Off);
                ui->lAccountType->setText(QString());
                break;
        }

        ui->lAccountImage->setIcon(icon);
        ui->lAccountImage->setIconSize(QSize(32, 32));

        MegaNode *node = megaApi->getNodeByHandle(preferences->uploadFolder());
        if (!node)
        {
            hasDefaultUploadOption = false;
            ui->eUploadFolder->setText(QString::fromUtf8("/MEGAsync Uploads"));
        }
        else
        {
            const char *nPath = megaApi->getNodePath(node);
            if (!nPath)
            {
                hasDefaultUploadOption = false;
                ui->eUploadFolder->setText(QString::fromUtf8("/MEGAsync Uploads"));
            }
            else
            {
                hasDefaultUploadOption = preferences->hasDefaultUploadFolder();
                ui->eUploadFolder->setText(QString::fromUtf8(nPath));
                delete [] nPath;
            }
        }
        delete node;

        QString downloadPath = preferences->downloadFolder();
        if (!downloadPath.size())
        {
            downloadPath = Utilities::getDefaultBasePath() + QString::fromUtf8("/MEGAsync Downloads");
        }
        downloadPath = QDir::toNativeSeparators(downloadPath);
        ui->eDownloadFolder->setText(downloadPath);
        hasDefaultDownloadOption = preferences->hasDefaultDownloadFolder();

        //Syncs
        loadSyncSettings();
#ifdef _WIN32
        ui->cDisableIcons->setChecked(preferences->leftPaneIconsDisabled());
#endif

        //Bandwidth
        ui->rUploadAutoLimit->setChecked(preferences->uploadLimitKB()<0);
        ui->rUploadLimit->setChecked(preferences->uploadLimitKB()>0);
        ui->rUploadNoLimit->setChecked(preferences->uploadLimitKB()==0);
        ui->eUploadLimit->setText((preferences->uploadLimitKB()<=0)? QString::fromLatin1("0") : QString::number(preferences->uploadLimitKB()));
        ui->eUploadLimit->setEnabled(ui->rUploadLimit->isChecked());

        ui->rDownloadLimit->setChecked(preferences->downloadLimitKB()>0);
        ui->rDownloadNoLimit->setChecked(preferences->downloadLimitKB()==0);
        ui->eDownloadLimit->setText((preferences->downloadLimitKB()<=0)? QString::fromLatin1("0") : QString::number(preferences->downloadLimitKB()));
        ui->eDownloadLimit->setEnabled(ui->rDownloadLimit->isChecked());

        ui->eMaxDownloadConnections->setValue(preferences->parallelDownloadConnections());
        ui->eMaxUploadConnections->setValue(preferences->parallelUploadConnections());

        ui->cbUseHttps->setChecked(preferences->usingHttpsOnly());

        if (accType == Preferences::ACCOUNT_TYPE_FREE) //Free user
        {
            ui->gBandwidthQuota->show();
            ui->bSeparatorBandwidth->show();
            ui->pUsedBandwidth->show();
            ui->pUsedBandwidth->setValue(0);
            ui->lBandwidth->setText(tr("Used quota for the last %1 hours: %2")
                    .arg(preferences->bandwidthInterval())
                    .arg(Utilities::getSizeString(preferences->usedBandwidth())));
        }
        else if (accType == Preferences::ACCOUNT_TYPE_BUSINESS)
        {
            ui->gBandwidthQuota->show();
            ui->bSeparatorBandwidth->show();
            ui->pUsedBandwidth->hide();
            ui->lBandwidth->setText(tr("%1 used")
                  .arg(Utilities::getSizeString(preferences->usedBandwidth())));
        }
        else
        {
            auto totalBandwidth = preferences->totalBandwidth();
            if (totalBandwidth == 0)
            {
                ui->gBandwidthQuota->hide();
                ui->bSeparatorBandwidth->hide();
                ui->pUsedBandwidth->show();
                ui->pUsedBandwidth->setValue(0);
                ui->lBandwidth->setText(tr("Data temporarily unavailable"));
            }
            else
            {
                ui->gBandwidthQuota->show();
                ui->bSeparatorBandwidth->show();
                auto bandwidthPercentage = Utilities::percentage(preferences->usedBandwidth(),preferences->totalBandwidth());
                ui->pUsedBandwidth->show();
                ui->pUsedBandwidth->setValue(bandwidthPercentage);
                ui->lBandwidth->setText(tr("%1 (%2%) of %3 used")
                        .arg(Utilities::getSizeString(preferences->usedBandwidth()))
                        .arg(QString::number(bandwidthPercentage))
                        .arg(Utilities::getSizeString(preferences->totalBandwidth())));
            }
        }

        //Advanced
        ui->lExcludedNames->clear();
        QStringList excludedNames = preferences->getExcludedSyncNames();
        for (int i = 0; i < excludedNames.size(); i++)
        {
            ui->lExcludedNames->addItem(excludedNames[i]);
        }

        QStringList excludedPaths = preferences->getExcludedSyncPaths();
        for (int i = 0; i < excludedPaths.size(); i++)
        {
            ui->lExcludedNames->addItem(excludedPaths[i]);
        }

        loadSizeLimits();
        ui->cDisableFileVersioning->setChecked(preferences->fileVersioningDisabled());
        ui->cOverlayIcons->setChecked(preferences->overlayIconsDisabled());
    }

    if (!proxyTestProgressDialog)
    {
        //Proxies
        ui->rNoProxy->setChecked(preferences->proxyType()==Preferences::PROXY_TYPE_NONE);
        ui->rProxyAuto->setChecked(preferences->proxyType()==Preferences::PROXY_TYPE_AUTO);
        ui->rProxyManual->setChecked(preferences->proxyType()==Preferences::PROXY_TYPE_CUSTOM);
        ui->cProxyType->setCurrentIndex(preferences->proxyProtocol());
        ui->eProxyServer->setText(preferences->proxyServer());
        ui->eProxyPort->setText(QString::number(preferences->proxyPort()));

        ui->cProxyRequiresPassword->setChecked(preferences->proxyRequiresAuth());
        ui->eProxyUsername->setText(preferences->getProxyUsername());
        ui->eProxyPassword->setText(preferences->getProxyPassword());

        if (ui->rProxyManual->isChecked())
        {
            ui->cProxyType->setEnabled(true);
            ui->eProxyServer->setEnabled(true);
            ui->eProxyPort->setEnabled(true);
            ui->cProxyRequiresPassword->setEnabled(true);
        }
        else
        {
            ui->cProxyType->setEnabled(false);
            ui->eProxyServer->setEnabled(false);
            ui->eProxyPort->setEnabled(false);
            ui->cProxyRequiresPassword->setEnabled(false);
        }

        if (ui->cProxyRequiresPassword->isChecked())
        {
            ui->eProxyUsername->setEnabled(true);
            ui->eProxyPassword->setEnabled(true);
        }
        else
        {
            ui->eProxyUsername->setEnabled(false);
            ui->eProxyPassword->setEnabled(false);
        }
    }

    ui->bApply->setEnabled(false);
    this->update();
    modifyingSettings--;
}

void SettingsDialog::refreshAccountDetails()
{

    if (preferences->accountType() == Preferences::ACCOUNT_TYPE_BUSINESS)
    {
        ui->pStorage->hide();
        ui->pUsedBandwidth->hide();
    }
    else
    {
        ui->pStorage->show();
        ui->pUsedBandwidth->show();
    }

    if (preferences->totalStorage() == 0)
    {
        ui->pStorage->setValue(0);
        ui->lStorage->setText(tr("Data temporarily unavailable"));
        ui->bStorageDetails->setEnabled(false);
    }
    else
    {
        ui->bStorageDetails->setEnabled(true);

        if (preferences->accountType() == Preferences::ACCOUNT_TYPE_BUSINESS)
        {
            ui->lStorage->setText(tr("%1 used")
                  .arg(Utilities::getSizeString(preferences->usedStorage())));
        }
        else
        {
            auto percentage = Utilities::percentage(preferences->usedStorage(),preferences->totalStorage());
            ui->pStorage->setValue(percentage);
            ui->lStorage->setText(tr("%1 (%2%) of %3 used")
                  .arg(Utilities::getSizeString(preferences->usedStorage()))
                  .arg(QString::number(percentage))
                  .arg(Utilities::getSizeString(preferences->totalStorage())));
        }
    }

    if (preferences->totalBandwidth() == 0)
    {
        ui->pUsedBandwidth->setValue(0);
        ui->lBandwidth->setText(tr("Data temporarily unavailable"));
    }
    else
    {
        if (preferences->accountType() == Preferences::ACCOUNT_TYPE_BUSINESS)
        {
            ui->lBandwidth->setText(tr("%1 used")
                  .arg(Utilities::getSizeString(preferences->usedBandwidth())));
        }
        else
        {
            auto percentage = Utilities::percentage(preferences->usedBandwidth(),preferences->totalBandwidth());
            ui->pUsedBandwidth->setValue(percentage);
            ui->lBandwidth->setText(tr("%1 (%2%) of %3 used")
                  .arg(Utilities::getSizeString(preferences->usedBandwidth()))
                  .arg(QString::number(percentage))
                  .arg(Utilities::getSizeString(preferences->totalBandwidth())));
        }
    }

    if (accountDetailsDialog)
    {
        accountDetailsDialog->refresh(preferences);
    }
}

int SettingsDialog::saveSettings()
{
    modifyingSettings++;
    if (!proxyOnly)
    {
        //General
        preferences->setShowNotifications(ui->cShowNotifications->isChecked());

        if (ui->cAutoUpdate->isEnabled())
        {
            bool updateAutomatically = ui->cAutoUpdate->isChecked();
            if (updateAutomatically != preferences->updateAutomatically())
            {
                preferences->setUpdateAutomatically(updateAutomatically);
                if (updateAutomatically)
                {
                    on_bUpdate_clicked();
                }
            }
        }

        bool startOnStartup = ui->cStartOnStartup->isChecked();
        if (!Platform::startOnStartup(startOnStartup))
        {
            // in case of failure - make sure configuration keeps the right value
            //LOG_debug << "Failed to " << (startOnStartup ? "enable" : "disable") << " MEGASync on startup.";
            preferences->setStartOnStartup(!startOnStartup);
        }
        else
        {
            preferences->setStartOnStartup(startOnStartup);
        }

        //Language
        int currentIndex = ui->cLanguage->currentIndex();
        QString selectedLanguageCode = languageCodes[currentIndex];
        if (preferences->language() != selectedLanguageCode)
        {
            preferences->setLanguage(selectedLanguageCode);
            app->changeLanguage(selectedLanguageCode);
            QString currentLanguageCode = app->getCurrentLanguageCode();
            megaApi->setLanguage(currentLanguageCode.toUtf8().constData());
            megaApi->setLanguagePreference(currentLanguageCode.toUtf8().constData());
        }

        //Account
        MegaNode *node = megaApi->getNodeByPath(ui->eUploadFolder->text().toUtf8().constData());
        if (node)
        {
            preferences->setHasDefaultUploadFolder(hasDefaultUploadOption);
            preferences->setUploadFolder(node->getHandle());
        }
        else
        {
            preferences->setHasDefaultUploadFolder(false);
            preferences->setUploadFolder(0);
        }
        delete node;

        QString defaultDownloadPath = Utilities::getDefaultBasePath() + QString::fromUtf8("/MEGAsync Downloads");
        if (ui->eDownloadFolder->text().compare(QDir::toNativeSeparators(defaultDownloadPath))
                || preferences->downloadFolder().size())
        {
            preferences->setDownloadFolder(ui->eDownloadFolder->text());
        }

        preferences->setHasDefaultDownloadFolder(hasDefaultDownloadOption);

        //Syncs
        if (syncsChanged)
        {
            //Check for removed or disabled folders
            for (int i = 0; i < preferences->getNumSyncedFolders(); i++)
            {
                QString localPath = preferences->getLocalFolder(i);
                QString megaPath = preferences->getMegaFolder(i);
                MegaHandle megaHandle = preferences->getMegaFolderHandle(i);

                int j;
                for (j = 0; j < ui->tSyncs->rowCount(); j++)
                {
                    QString newLocalPath = ui->tSyncs->item(j, 0)->text();
                    QString newMegaPath = ui->tSyncs->item(j, 1)->text();
                    bool enabled = ((QCheckBox *)ui->tSyncs->cellWidget(j, 2))->isChecked();

                    if (!megaPath.compare(newMegaPath) && !localPath.compare(newLocalPath))
                    {
                        if (!enabled && preferences->isFolderActive(i) != enabled)
                        {
                            Platform::syncFolderRemoved(preferences->getLocalFolder(i),
                                                        preferences->getSyncName(i),
                                                        preferences->getSyncID(i));
                            preferences->setSyncState(i, enabled);

                            MegaNode *node = megaApi->getNodeByHandle(megaHandle);
                            megaApi->removeSync(node);
                            delete node;
                        }
                        break;
                    }
                }

                if (j == ui->tSyncs->rowCount())
                {
                    MegaApi::log(MegaApi::LOG_LEVEL_INFO, QString::fromLatin1("Removing sync: %1").arg(preferences->getSyncName(i)).toUtf8().constData());
                    bool active = preferences->isFolderActive(i);
                    MegaNode *node = megaApi->getNodeByHandle(megaHandle);
                    if (active)
                    {
                        Platform::syncFolderRemoved(preferences->getLocalFolder(i),
                                                    preferences->getSyncName(i),
                                                    preferences->getSyncID(i));
                        megaApi->removeSync(node);
                    }
                    preferences->removeSyncedFolder(i);
                    delete node;
                    i--;
                }
            }

            //Check for new or enabled folders
            for (int i = 0; i < ui->tSyncs->rowCount(); i++)
            {
                QString localFolderPath = ui->tSyncs->item(i, 0)->text();
                QString megaFolderPath = ui->tSyncs->item(i, 1)->text();
                bool enabled = ((QCheckBox *)ui->tSyncs->cellWidget(i, 2))->isChecked();

                QString syncName = syncNames.at(i);
                MegaNode *node = megaApi->getNodeByPath(megaFolderPath.toUtf8().constData());
                if (!node)
                {
                    if (enabled)
                    {
                        ((QCheckBox *)ui->tSyncs->cellWidget(i, 2))->setChecked(false);
                    }

                    continue;
                }

                int j;

                QFileInfo localFolderInfo(localFolderPath);
                localFolderPath = QDir::toNativeSeparators(localFolderInfo.canonicalFilePath());
                if (!localFolderPath.size() || !localFolderInfo.isDir())
                {
                    if (enabled)
                    {
                        ((QCheckBox *)ui->tSyncs->cellWidget(i, 2))->setChecked(false);
                    }

                    continue;
                }

                for (j = 0; j < preferences->getNumSyncedFolders(); j++)
                {
                    QString previousLocalPath = preferences->getLocalFolder(j);
                    QString previousMegaPath = preferences->getMegaFolder(j);

                    if (!megaFolderPath.compare(previousMegaPath) && !localFolderPath.compare(previousLocalPath))
                    {
                        if (enabled && preferences->isFolderActive(j) != enabled)
                        {
                            preferences->setMegaFolderHandle(j, node->getHandle());
                            preferences->setSyncState(j, enabled);
                            megaApi->syncFolder(localFolderPath.toUtf8().constData(), node);
                        }
                        break;
                    }
                }

                if (j == preferences->getNumSyncedFolders())
                {
                    MegaApi::log(MegaApi::LOG_LEVEL_INFO, QString::fromLatin1("Adding sync: %1 - %2")
                                 .arg(localFolderPath).arg(megaFolderPath).toUtf8().constData());

                    preferences->addSyncedFolder(localFolderPath,
                                                 megaFolderPath,
                                                 node->getHandle(),
                                                 syncName, enabled);

                    if (enabled)
                    {
                        megaApi->syncFolder(localFolderPath.toUtf8().constData(), node);
                    }
                }
                delete node;
            }

            app->createTrayMenu();
            syncsChanged = false;
        }
#ifdef _WIN32
        bool iconsDisabled = ui->cDisableIcons->isChecked();
        if (preferences->leftPaneIconsDisabled() != iconsDisabled)
        {
            if (iconsDisabled)
            {
                Platform::removeAllSyncsFromLeftPane();
            }
            else
            {
                for (int i = 0; i < preferences->getNumSyncedFolders(); i++)
                {
                    Platform::addSyncToLeftPane(preferences->getLocalFolder(i),
                                                preferences->getSyncName(i),
                                                preferences->getSyncID(i));
                }
            }
            preferences->disableLeftPaneIcons(iconsDisabled);
        }
#endif

#ifndef WIN32
        if (permissionsChanged)
        {
            megaApi->setDefaultFilePermissions(filePermissions);
            filePermissions = megaApi->getDefaultFilePermissions();
            preferences->setFilePermissionsValue(filePermissions);

            megaApi->setDefaultFolderPermissions(folderPermissions);
            folderPermissions = megaApi->getDefaultFolderPermissions();
            preferences->setFolderPermissionsValue(folderPermissions);

            permissionsChanged = false;
            filePermissions   = 0;
            folderPermissions = 0;
        }
#endif

        //Bandwidth
        if (ui->rUploadLimit->isChecked())
        {
            preferences->setUploadLimitKB(ui->eUploadLimit->text().trimmed().toInt());
            app->setUploadLimit(0);
        }
        else
        {
            if (ui->rUploadNoLimit->isChecked())
            {
                preferences->setUploadLimitKB(0);
            }
            else if (ui->rUploadAutoLimit->isChecked())
            {
                preferences->setUploadLimitKB(-1);
            }

            app->setUploadLimit(preferences->uploadLimitKB());
        }
        app->setMaxUploadSpeed(preferences->uploadLimitKB());

        if (ui->rDownloadNoLimit->isChecked())
        {
            preferences->setDownloadLimitKB(0);
        }
        else
        {
            preferences->setDownloadLimitKB(ui->eDownloadLimit->text().trimmed().toInt());
        }

        app->setMaxDownloadSpeed(preferences->downloadLimitKB());

        if (ui->eMaxDownloadConnections->value() != preferences->parallelDownloadConnections())
        {
            preferences->setParallelDownloadConnections(ui->eMaxDownloadConnections->value());
            app->setMaxConnections(MegaTransfer::TYPE_DOWNLOAD, preferences->parallelDownloadConnections());
        }

        if (ui->eMaxUploadConnections->value() != preferences->parallelUploadConnections())
        {
            preferences->setParallelUploadConnections(ui->eMaxUploadConnections->value());
            app->setMaxConnections(MegaTransfer::TYPE_UPLOAD, preferences->parallelUploadConnections());
        }

        preferences->setUseHttpsOnly(ui->cbUseHttps->isChecked());
        app->setUseHttpsOnly(preferences->usingHttpsOnly());

        if (sizeLimitsChanged)
        {
            preferences->setUpperSizeLimit(hasUpperLimit);
            preferences->setLowerSizeLimit(hasLowerLimit);
            preferences->setUpperSizeLimitValue(upperLimit);
            preferences->setLowerSizeLimitValue(lowerLimit);
            preferences->setUpperSizeLimitUnit(upperLimitUnit);
            preferences->setLowerSizeLimitUnit(lowerLimitUnit);
            preferences->setCrashed(true);
            QMegaMessageBox::information(this, tr("Warning"),
                                         tr("The new excluded file sizes will be taken into account when the application starts again."),
                                         Utilities::getDevicePixelRatio(),
                                         QMessageBox::Ok);
            sizeLimitsChanged = false;
        }

        if (cleanerLimitsChanged)
        {
            preferences->setCleanerDaysLimit(hasDaysLimit);
            preferences->setCleanerDaysLimitValue(daysLimit);
            app->cleanLocalCaches();
            cleanerLimitsChanged = false;
        }

        if (fileVersioningChanged && ui->cDisableFileVersioning->isChecked() != preferences->fileVersioningDisabled())
        {
            megaApi->setFileVersionsOption(ui->cDisableFileVersioning->isChecked());
            fileVersioningChanged = false;
        }

        if (ui->cOverlayIcons->isChecked() != preferences->overlayIconsDisabled())
        {
            preferences->disableOverlayIcons(ui->cOverlayIcons->isChecked());
            #ifdef Q_OS_MACX
            Platform::notifyRestartSyncFolders();
            #else
            for (int i = 0; i < preferences->getNumSyncedFolders(); i++)
            {
                app->notifyItemChange(preferences->getLocalFolder(i), MegaApi::STATE_NONE);
            }
            #endif
        }
    }

    int proxyChanged = 0;
    //Proxies
    if (!proxyTestProgressDialog && ((ui->rNoProxy->isChecked() && (preferences->proxyType() != Preferences::PROXY_TYPE_NONE))       ||
        (ui->rProxyAuto->isChecked() &&  (preferences->proxyType() != Preferences::PROXY_TYPE_AUTO))    ||
        (ui->rProxyManual->isChecked() &&  (preferences->proxyType() != Preferences::PROXY_TYPE_CUSTOM))||
        (preferences->proxyProtocol() != ui->cProxyType->currentIndex())                                ||
        (preferences->proxyServer() != ui->eProxyServer->text().trimmed())                              ||
        (preferences->proxyPort() != ui->eProxyPort->text().toInt())                                    ||
        (preferences->proxyRequiresAuth() != ui->cProxyRequiresPassword->isChecked())                   ||
        (preferences->getProxyUsername() != ui->eProxyUsername->text())                                 ||
        (preferences->getProxyPassword() != ui->eProxyPassword->text())))
    {
        proxyChanged = 1;
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::NoProxy);
        if (ui->rProxyManual->isChecked())
        {
            switch(ui->cProxyType->currentIndex())
            {
            case Preferences::PROXY_PROTOCOL_SOCKS5H:
                proxy.setType(QNetworkProxy::Socks5Proxy);
                break;
            default:
                proxy.setType(QNetworkProxy::HttpProxy);
                break;
            }

            proxy.setHostName(ui->eProxyServer->text().trimmed());
            proxy.setPort(ui->eProxyPort->text().trimmed().toUShort());
            if (ui->cProxyRequiresPassword->isChecked())
            {
                proxy.setUser(ui->eProxyUsername->text());
                proxy.setPassword(ui->eProxyPassword->text());
            }
        }
        else if (ui->rProxyAuto->isChecked())
        {
            MegaProxy *proxySettings = megaApi->getAutoProxySettings();
            if (proxySettings->getProxyType()==MegaProxy::PROXY_CUSTOM)
            {
                string sProxyURL = proxySettings->getProxyURL();
                QString proxyURL = QString::fromUtf8(sProxyURL.data());

                QStringList parts = proxyURL.split(QString::fromLatin1("://"));
                if (parts.size() == 2 && parts[0].startsWith(QString::fromUtf8("socks")))
                {
                    proxy.setType(QNetworkProxy::Socks5Proxy);
                }
                else
                {
                    proxy.setType(QNetworkProxy::HttpProxy);
                }

                QStringList arguments = parts[parts.size()-1].split(QString::fromLatin1(":"));
                if (arguments.size() == 2)
                {
                    proxy.setHostName(arguments[0]);
                    proxy.setPort(arguments[1].toUShort());
                }
            }
            delete proxySettings;
        }

        proxyTestProgressDialog = new MegaProgressDialog(tr("Please wait..."), QString(), 0, 0, this, Qt::CustomizeWindowHint|Qt::WindowTitleHint);
        proxyTestProgressDialog->setWindowModality(Qt::WindowModal);
        proxyTestProgressDialog->show();

        ConnectivityChecker *connectivityChecker = new ConnectivityChecker(Preferences::PROXY_TEST_URL);
        connectivityChecker->setProxy(proxy);
        connectivityChecker->setTestString(Preferences::PROXY_TEST_SUBSTRING);
        connectivityChecker->setTimeout(Preferences::PROXY_TEST_TIMEOUT_MS);

        connect(connectivityChecker, SIGNAL(testError()), this, SLOT(onProxyTestError()));
        connect(connectivityChecker, SIGNAL(testSuccess()), this, SLOT(onProxyTestSuccess()));
        connect(connectivityChecker, SIGNAL(testFinished()), connectivityChecker, SLOT(deleteLater()));

        connectivityChecker->startCheck();
        MegaApi::log(MegaApi::LOG_LEVEL_INFO, "Testing proxy settings...");        
    }

    //Advanced
    if (excludedNamesChanged)
    {
        QStringList excludedNames;
        QStringList excludedPaths;
        for (int i = 0; i < ui->lExcludedNames->count(); i++)
        {
            if (ui->lExcludedNames->item(i)->text().contains(QDir::separator())) // Path exclusion
            {
                excludedPaths.append(ui->lExcludedNames->item(i)->text());
            }
            else
            {
                excludedNames.append(ui->lExcludedNames->item(i)->text()); // File name exclusion
            }
        }

        preferences->setExcludedSyncNames(excludedNames);
        preferences->setExcludedSyncPaths(excludedPaths);
        preferences->setCrashed(true);
        excludedNamesChanged = false;

        QMessageBox* info = new QMessageBox(QMessageBox::Warning, QString::fromLatin1("MEGAsync"),
                                            tr("The new excluded file names will be taken into account\n"
                                               "when the application starts again"));
        info->setStandardButtons(QMessageBox::Ok | QMessageBox::Yes);
        info->setButtonText(QMessageBox::Yes, tr("Restart"));
        info->setDefaultButton(QMessageBox::Ok);

        QPointer<SettingsDialog> currentDialog = this;
        info->exec();
        int result = info->result();
        delete info;
        if (!currentDialog)
        {
            return 2;
        }

        if (result == QMessageBox::Yes)
        {
            // Restart MEGAsync
#if defined(Q_OS_MACX) || QT_VERSION < 0x050000
            ((MegaApplication*)qApp)->rebootApplication(false);
#else
            QTimer::singleShot(0, [] () {((MegaApplication*)qApp)->rebootApplication(false); }); //we enqueue this call, so as not to close before properly handling the exit of Settings Dialog
#endif
            return 2;
        }

        //QT_TR_NOOP("Do you want to restart MEGAsync now?");
    }

    ui->bApply->setEnabled(false);
    modifyingSettings--;
    return !proxyChanged;
}

void SettingsDialog::on_bDelete_clicked()
{
    QList<QTableWidgetSelectionRange> selected = ui->tSyncs->selectedRanges();
    if (selected.size() == 0)
    {
        return;
    }

    int index = selected.first().topRow();
    ui->tSyncs->removeRow(index);
    syncNames.removeAt(index);

    syncsChanged = true;
    stateChanged();
}

void SettingsDialog::loadSyncSettings()
{
    ui->tSyncs->clearContents();
    syncNames.clear();

    ui->tSyncs->horizontalHeader()->setVisible(true);
    int numFolders = preferences->getNumSyncedFolders();
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    ui->tSyncs->horizontalHeader()->setResizeMode(QHeaderView::Fixed);
#else
    ui->tSyncs->horizontalHeader()->sectionResizeMode(QHeaderView::Fixed);
#endif
    ui->tSyncs->setRowCount(numFolders);
    ui->tSyncs->setColumnCount(3);
    ui->tSyncs->setColumnWidth(2, 21);

    for (int i = 0; i < numFolders; i++)
    {
        QTableWidgetItem *localFolder = new QTableWidgetItem();
        localFolder->setText(preferences->getLocalFolder(i));
        QTableWidgetItem *megaFolder = new QTableWidgetItem();
        megaFolder->setText(preferences->getMegaFolder(i));
        localFolder->setToolTip(preferences->getLocalFolder(i));
        ui->tSyncs->setItem(i, 0, localFolder);
        megaFolder->setToolTip(preferences->getMegaFolder(i));
        ui->tSyncs->setItem(i, 1, megaFolder);
        syncNames.append(preferences->getSyncName(i));
        QCheckBox *c = new QCheckBox();
        c->setChecked(preferences->isFolderActive(i));
        c->setToolTip(tr("Enable / disable"));
        connect(c, SIGNAL(stateChanged(int)), this, SLOT(syncStateChanged(int)));
        ui->tSyncs->setCellWidget(i, 2, c);
    }
}

void SettingsDialog::loadSizeLimits()
{
    hasUpperLimit = preferences->upperSizeLimit();
    hasLowerLimit = preferences->lowerSizeLimit();
    upperLimit = preferences->upperSizeLimitValue();
    lowerLimit = preferences->lowerSizeLimitValue();
    upperLimitUnit = preferences->upperSizeLimitUnit();
    lowerLimitUnit = preferences->lowerSizeLimitUnit();
    ui->lLimitsInfo->setText(getFormatString());
    hasDaysLimit = preferences->cleanerDaysLimit();
    daysLimit = preferences->cleanerDaysLimitValue();
    ui->lLocalCleanerState->setText(getFormatLimitDays());
}
#ifndef WIN32
void SettingsDialog::on_bPermissions_clicked()
{
    QPointer<PermissionsDialog> dialog = new PermissionsDialog(this);
    dialog->setFolderPermissions(folderPermissions ? folderPermissions : megaApi->getDefaultFolderPermissions());
    dialog->setFilePermissions(filePermissions ? filePermissions : megaApi->getDefaultFilePermissions());

    int result = dialog->exec();
    if (!dialog || result != QDialog::Accepted)
    {
        delete dialog;
        return;
    }

    filePermissions = dialog->filePermissions();
    folderPermissions = dialog->folderPermissions();
    delete dialog;

    if (filePermissions != preferences->filePermissionsValue() ||
       folderPermissions != preferences->folderPermissionsValue())
    {
        permissionsChanged = true;
        stateChanged();
    }
}
#endif
void SettingsDialog::on_bAdd_clicked()
{
    QStringList currentLocalFolders;
    QList<long long> currentMegaFolders;
    for (int i = 0; i < ui->tSyncs->rowCount(); i++)
    {
        QString localFolder = ui->tSyncs->item(i, 0)->text();
        currentLocalFolders.append(localFolder);

        QString newMegaPath = ui->tSyncs->item(i, 1)->text();
        MegaNode *n = megaApi->getNodeByPath(newMegaPath.toUtf8().constData());
        if (!n)
        {
            continue;
        }

        currentMegaFolders.append(n->getHandle());
        delete n;
    }

    QPointer<BindFolderDialog> dialog = new BindFolderDialog(app, syncNames, currentLocalFolders, currentMegaFolders, this);
    int result = dialog->exec();
    if (!dialog || result != QDialog::Accepted)
    {
        delete dialog;
        return;
    }

    QString localFolderPath = QDir::toNativeSeparators(QDir(dialog->getLocalFolder()).canonicalPath());
    MegaHandle handle = dialog->getMegaFolder();
    MegaNode *node = megaApi->getNodeByHandle(handle);
    if (!localFolderPath.length() || !node)
    {
        delete dialog;
        delete node;
        return;
    }

    QTableWidgetItem *localFolder = new QTableWidgetItem();
    localFolder->setText(localFolderPath);
    QTableWidgetItem *megaFolder = new QTableWidgetItem();
    const char *nPath = megaApi->getNodePath(node);
    if (!nPath)
    {
        delete dialog;
        delete node;
        return;
    }

    megaFolder->setText(QString::fromUtf8(nPath));
    int pos = ui->tSyncs->rowCount();
    ui->tSyncs->setRowCount(pos+1);
    localFolder->setToolTip(localFolderPath);
    ui->tSyncs->setItem(pos, 0, localFolder);
    megaFolder->setToolTip(QString::fromUtf8(nPath));
    ui->tSyncs->setItem(pos, 1, megaFolder);

    QCheckBox *c = new QCheckBox();
    c->setChecked(true);
    c->setToolTip(tr("Enable / disable"));
    connect(c, SIGNAL(stateChanged(int)), this, SLOT(syncStateChanged(int)));
    ui->tSyncs->setCellWidget(pos, 2, c);

    syncNames.append(dialog->getSyncName());
    delete [] nPath;
    delete dialog;
    delete node;

    syncsChanged = true;
    stateChanged();
}

void SettingsDialog::on_bApply_clicked()
{
    saveSettings();
}

void SettingsDialog::on_bUnlink_clicked()
{
    QPointer<SettingsDialog> currentDialog = this;
    if (QMessageBox::question(NULL, tr("Logout"),
            tr("Synchronization will stop working.") + QString::fromLatin1(" ") + tr("Are you sure?"),
            QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
    {
        if (currentDialog)
        {
            close();
            app->unlink();
        }
    }
}

void SettingsDialog::on_bExportMasterKey_clicked()
{
    QString defaultPath = QDir::toNativeSeparators(Utilities::getDefaultBasePath());
#ifndef _WIN32
    if (defaultPath.isEmpty())
    {
        defaultPath = QString::fromUtf8("/");
    }
#endif

    QDir dir(defaultPath);
    QString fileName = QFileDialog::getSaveFileName(0,
             tr("Export Master key"), dir.filePath(tr("MEGA-RECOVERYKEY")),
             QString::fromUtf8("Txt file (*.txt)"), NULL, QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QFile::Truncate))
    {
        QMegaMessageBox::information(this, tr("Unable to write file"), file.errorString(), Utilities::getDevicePixelRatio());
        return;
    }

    QTextStream out(&file);
    out << megaApi->exportMasterKey();

    file.close();

    megaApi->masterKeyExported();

    QMegaMessageBox::information(this, tr("Warning"),
                                 tr("Exporting the master key and keeping it in a secure location enables you to set a new password without data loss.") + QString::fromUtf8("\n")
                                 + tr("Always keep physical control of your master key (e.g. on a client device, external storage, or print)."),
                                 Utilities::getDevicePixelRatio(),
                                 QMessageBox::Ok);
}

void SettingsDialog::on_tSyncs_doubleClicked(const QModelIndex &index)
{
    if (!index.column())
    {
        QString localFolderPath = ui->tSyncs->item(index.row(), 0)->text();
        QtConcurrent::run(QDesktopServices::openUrl, QUrl::fromLocalFile(localFolderPath));
    }
    else
    {
        QString megaFolderPath = ui->tSyncs->item(index.row(), 1)->text();
        MegaNode *node = megaApi->getNodeByPath(megaFolderPath.toUtf8().constData());
        if (node)
        {
            const char *handle = node->getBase64Handle();
            QString url = Preferences::BASE_URL + QString::fromLatin1("/fm/") + QString::fromLatin1(handle);
            QtConcurrent::run(QDesktopServices::openUrl, QUrl(url));
            delete [] handle;
            delete node;
        }
    }
}

void SettingsDialog::on_bUploadFolder_clicked()
{
    QPointer<NodeSelector> nodeSelector = new NodeSelector(megaApi, NodeSelector::UPLOAD_SELECT, this);
    MegaNode *defaultNode = megaApi->getNodeByPath(ui->eUploadFolder->text().toUtf8().constData());
    if (defaultNode)
    {
        nodeSelector->setSelectedFolderHandle(defaultNode->getHandle());
        delete defaultNode;
    }

    nodeSelector->setDefaultUploadOption(hasDefaultUploadOption);
    nodeSelector->showDefaultUploadOption();
    int result = nodeSelector->exec();
    if (!nodeSelector || result != QDialog::Accepted)
    {
        delete nodeSelector;
        return;
    }

    MegaHandle selectedMegaFolderHandle = nodeSelector->getSelectedFolderHandle();
    MegaNode *node = megaApi->getNodeByHandle(selectedMegaFolderHandle);
    if (!node)
    {
        delete nodeSelector;
        return;
    }

    const char *nPath = megaApi->getNodePath(node);
    if (!nPath)
    {
        delete nodeSelector;
        delete node;
        return;
    }

    QString newPath = QString::fromUtf8(nPath);
    if (newPath.compare(ui->eUploadFolder->text()) || hasDefaultUploadOption != nodeSelector->getDefaultUploadOption())
    {
        hasDefaultUploadOption = nodeSelector->getDefaultUploadOption();
        ui->eUploadFolder->setText(newPath);
        stateChanged();
    }

    delete nodeSelector;
    delete [] nPath;
    delete node;
}

void SettingsDialog::on_bDownloadFolder_clicked()
{
    QPointer<DownloadFromMegaDialog> dialog = new DownloadFromMegaDialog(preferences->downloadFolder(), this);
    dialog->setDefaultDownloadOption(hasDefaultDownloadOption);

    int result = dialog->exec();
    if (!dialog || result != QDialog::Accepted)
    {
        delete dialog;
        return;
    }

    QString fPath = dialog->getPath();
    if (fPath.size() && (fPath.compare(ui->eDownloadFolder->text())
            || (hasDefaultDownloadOption != dialog->isDefaultDownloadOption())))
    {
        QTemporaryFile test(fPath + QDir::separator());
        if (!test.open())
        {
            QMessageBox::critical(NULL, tr("Error"), tr("You don't have write permissions in this local folder."));
            delete dialog;
            return;
        }

        hasDefaultDownloadOption = dialog->isDefaultDownloadOption();
        ui->eDownloadFolder->setText(fPath);
        stateChanged();
    }

    delete dialog;
}

void SettingsDialog::on_bAddName_clicked()
{
    QPointer<AddExclusionDialog> add = new AddExclusionDialog(this);
    int result = add->exec();
    if (!add || result != QDialog::Accepted)
    {
        delete add;
        return;
    }

    QString text = add->textValue();
    delete add;

    if (text.isEmpty())
    {
        return;
    }

    for (int i = 0; i < ui->lExcludedNames->count(); i++)
    {
        if (ui->lExcludedNames->item(i)->text() == text)
        {
            return;
        }
        else if (ui->lExcludedNames->item(i)->text().compare(text, Qt::CaseInsensitive) > 0)
        {
            ui->lExcludedNames->insertItem(i, text);
            excludedNamesChanged = true;
            stateChanged();
            return;
        }
    }

    ui->lExcludedNames->addItem(text);
    excludedNamesChanged = true;
    stateChanged();
}

void SettingsDialog::on_bDeleteName_clicked()
{
    QList<QListWidgetItem *> selected = ui->lExcludedNames->selectedItems();
    if (selected.size() == 0)
    {
        return;
    }

    for (int i = 0; i < selected.size(); i++)
    {
        delete selected[i];
    }

    excludedNamesChanged = true;
    stateChanged();
}

void SettingsDialog::on_bExcludeSize_clicked()
{
    QPointer<SizeLimitDialog> dialog = new SizeLimitDialog(this);
    dialog->setUpperSizeLimit(hasUpperLimit);
    dialog->setLowerSizeLimit(hasLowerLimit);
    dialog->setUpperSizeLimitValue(upperLimit);
    dialog->setLowerSizeLimitValue(lowerLimit);
    dialog->setUpperSizeLimitUnit(upperLimitUnit);
    dialog->setLowerSizeLimitUnit(lowerLimitUnit);

    int result = dialog->exec();
    if (!dialog || result != QDialog::Accepted)
    {
        delete dialog;
        return;
    }

    hasUpperLimit = dialog->upperSizeLimit();
    hasLowerLimit = dialog->lowerSizeLimit();
    upperLimit = dialog->upperSizeLimitValue();
    lowerLimit = dialog->lowerSizeLimitValue();
    upperLimitUnit = dialog->upperSizeLimitUnit();
    lowerLimitUnit = dialog->lowerSizeLimitUnit();
    delete dialog;

    ui->lLimitsInfo->setText(getFormatString());
    if (hasUpperLimit != preferences->upperSizeLimit() ||
       hasLowerLimit != preferences->lowerSizeLimit() ||
       upperLimit != preferences->upperSizeLimitValue() ||
       lowerLimit != preferences->lowerSizeLimitValue() ||
       upperLimitUnit != preferences->upperSizeLimitUnit() ||
       lowerLimitUnit != preferences->lowerSizeLimitUnit())
    {
        sizeLimitsChanged = true;
        stateChanged();
    }
}

void SettingsDialog::on_bLocalCleaner_clicked()
{
    QPointer<LocalCleanScheduler> dialog = new LocalCleanScheduler(this);
    dialog->setDaysLimit(hasDaysLimit);
    dialog->setDaysLimitValue(daysLimit);

    int result = dialog->exec();
    if (!dialog || result != QDialog::Accepted)
    {
        delete dialog;
        return;
    }

    hasDaysLimit = dialog->daysLimit();
    daysLimit = dialog->daysLimitValue();
    delete dialog;

    ui->lLocalCleanerState->setText(getFormatLimitDays());
    if (hasDaysLimit != preferences->cleanerDaysLimit() ||
       daysLimit != preferences->cleanerDaysLimitValue())
    {
        cleanerLimitsChanged = true;
        stateChanged();
    }
}

void SettingsDialog::changeEvent(QEvent *event)
{
    modifyingSettings++;
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        ui->bLocalCleaner->setText(ui->bLocalCleaner->text().arg(QString::fromLatin1(MEGA_DEBRIS_FOLDER)));
        ui->lFileVersionsSize->setText(tr("File versions: %1").arg(Utilities::getSizeString(fileVersionsSize)));


#ifdef __APPLE__
        setWindowTitle(tr("Preferences - MEGAsync"));
        ui->cStartOnStartup->setText(tr("Open at login"));
#endif
        ui->cProxyType->addItem(QString::fromUtf8("SOCKS5H"));

        loadSettings();
        onCacheSizeAvailable();
    }
    QDialog::changeEvent(event);
    modifyingSettings--;
}

QString SettingsDialog::getFormatString()
{
    QString format;
    if (hasLowerLimit || hasUpperLimit)
    {
        format += QString::fromUtf8("(");

        if (hasLowerLimit)
        {
            format  += QString::fromUtf8("<") + Utilities::getSizeString(lowerLimit * (1 << (10 * lowerLimitUnit)));
        }

        if (hasLowerLimit && hasUpperLimit)
        {
            format  += QString::fromUtf8(", ");
        }

        if (hasUpperLimit)
        {
            format  += QString::fromUtf8(">") + Utilities::getSizeString(upperLimit * (1 << (10 * upperLimitUnit)));
        }

        format += QString::fromUtf8(")");
    }
    else
    {
        format = tr("Disabled");
    }
    return format;
}

QString SettingsDialog::getFormatLimitDays()
{
    QString format;
    if (hasDaysLimit)
    {
        if (daysLimit > 1)
        {
            format += tr("Remove files older than %1 days").arg(QString::number(daysLimit));
        }
        else
        {
            format += tr("Remove files older than 1 day");
        }
    }
    else
    {
        format = tr("Disabled");
    }
    return format;
}

void SettingsDialog::on_bClearCache_clicked()
{
    QString syncs;
    int numFolders = preferences->getNumSyncedFolders();
    for (int i = 0; i < numFolders; i++)
    {
        QFileInfo fi(preferences->getLocalFolder(i) + QDir::separator() + QString::fromLatin1(MEGA_DEBRIS_FOLDER));
        if (fi.exists() && fi.isDir())
        {
            syncs += QString::fromUtf8("<br/><a href=\"local://#%1\">%2</a>")
                    .arg(fi.absoluteFilePath() + QDir::separator()).arg(preferences->getSyncName(i));
        }
    }

    QPointer<QMessageBox> warningDel = new QMessageBox(this);
    warningDel->setIcon(QMessageBox::Warning);
    warningDel->setIconPixmap(QPixmap(Utilities::getDevicePixelRatio() < 2 ? QString::fromUtf8(":/images/mbox-warning.png")
                                                                : QString::fromUtf8(":/images/mbox-warning@2x.png")));

    warningDel->setWindowTitle(tr("Clear local backup"));
    warningDel->setTextFormat(Qt::RichText);

#if QT_VERSION > 0x050100
    warningDel->setTextInteractionFlags(Qt::NoTextInteraction | Qt::LinksAccessibleByMouse);
#endif

    warningDel->setText(tr("Backups of the previous versions of your synced files in your computer will be permanently deleted. "
                           "Please, check your backup folders to see if you need to rescue something before continuing:")
                           + QString::fromUtf8("<br/>") + syncs
                           + QString::fromUtf8("<br/><br/>") + tr("Do you want to delete your local backup now?"));
    warningDel->setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    warningDel->setDefaultButton(QMessageBox::No);
    int result = warningDel->exec();
    if (!warningDel || result != QMessageBox::Yes)
    {
        delete warningDel;
        return;
    }
    delete warningDel;

    QtConcurrent::run(deleteCache);

    cacheSize = 0;

    ui->bClearCache->hide();
    ui->lCacheSize->hide();

    // Move remote SyncDebris widget to left side
    ui->gCache->layout()->removeWidget(ui->wLocalCache);
    ui->wRemoteCache->layout()->removeItem(ui->rSpacer);
#ifndef __APPLE__
    ui->lRemoteCacheSize->setMargin(2);
#endif
    ((QBoxLayout *)ui->gCache->layout())->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Fixed));

    onClearCache();
}

void SettingsDialog::on_bClearRemoteCache_clicked()
{
    MegaNode *syncDebris = megaApi->getNodeByPath("//bin/SyncDebris");
    if (!syncDebris)
    {
        remoteCacheSize = 0;
        ui->bClearRemoteCache->hide();
        ui->lRemoteCacheSize->hide();
        onClearCache();
        return;
    }

    QPointer<QMessageBox> warningDel = new QMessageBox(this);
    warningDel->setIcon(QMessageBox::Warning);
    warningDel->setIconPixmap(QPixmap(Utilities::getDevicePixelRatio() < 2 ? QString::fromUtf8(":/images/mbox-warning.png")
                                                                : QString::fromUtf8(":/images/mbox-warning@2x.png")));
    warningDel->setWindowTitle(tr("Clear remote backup"));
    warningDel->setTextFormat(Qt::RichText);

#if QT_VERSION > 0x050100
    warningDel->setTextInteractionFlags(Qt::NoTextInteraction | Qt::LinksAccessibleByMouse);
#endif

    char *base64Handle = syncDebris->getBase64Handle();
    warningDel->setText(tr("Backups of the previous versions of your synced files in MEGA will be permanently deleted. "
                           "Please, check your [A] folder in the Rubbish Bin of your MEGA account to see if you need to rescue something before continuing.")
                           .replace(QString::fromUtf8("[A]"), QString::fromUtf8("<a href=\"mega://#fm/%1\">SyncDebris</a>").arg(QString::fromUtf8(base64Handle)))
                           + QString::fromUtf8("<br/><br/>") + tr("Do you want to delete your remote backup now?"));
    delete [] base64Handle;

    warningDel->setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    warningDel->setDefaultButton(QMessageBox::No);
    int result = warningDel->exec();
    if (!warningDel || result != QMessageBox::Yes)
    {
        delete warningDel;
        delete syncDebris;
        return;
    }
    delete warningDel;
    delete syncDebris;

    QtConcurrent::run(deleteRemoteCache, megaApi);
    remoteCacheSize = 0;

    ui->bClearRemoteCache->hide();
    ui->lRemoteCacheSize->hide();

    onClearCache();
}

void SettingsDialog::on_bClearFileVersions_clicked()
{
    QPointer<SettingsDialog> dialog = QPointer<SettingsDialog>(this);
    if (QMegaMessageBox::warning(NULL,
                             QString::fromUtf8("MEGAsync"),
                             tr("You are about to permanently remove all file versions. Would you like to proceed?"),
                             Utilities::getDevicePixelRatio(), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes
            || !dialog)
    {
        return;
    }

    megaApi->removeVersions();
    // Reset file version size to adjust UI
    fileVersionsSize = 0;

    ui->lFileVersionsSize->hide();
    ui->bClearFileVersions->hide();
}

void SettingsDialog::onClearCache()
{
    if (!cacheSize && !remoteCacheSize)
    {
        ui->gCache->setVisible(false);

    #ifdef __APPLE__
        if (!cacheSize && !remoteCacheSize)
        {
            minHeightAnimation->setTargetObject(this);
            maxHeightAnimation->setTargetObject(this);
            minHeightAnimation->setPropertyName("minimumHeight");
            maxHeightAnimation->setPropertyName("maximumHeight");
            minHeightAnimation->setStartValue(minimumHeight());
            maxHeightAnimation->setStartValue(maximumHeight());
            minHeightAnimation->setEndValue(595);
            maxHeightAnimation->setEndValue(595);
            minHeightAnimation->setDuration(150);
            maxHeightAnimation->setDuration(150);
            animationGroup->start();
        }
    #endif
    }
}
void SettingsDialog::onProxyTestError()
{
    MegaApi::log(MegaApi::LOG_LEVEL_WARNING, "Proxy test failed");
    if (proxyTestProgressDialog)
    {
        proxyTestProgressDialog->hide();
        delete proxyTestProgressDialog;
        proxyTestProgressDialog = NULL;
        ui->bApply->setEnabled(true);
        QMessageBox::critical(NULL, tr("Error"), tr("Your proxy settings are invalid or the proxy doesn't respond"));
    }

    shouldClose = false;
}

void SettingsDialog::onProxyTestSuccess()
{
    MegaApi::log(MegaApi::LOG_LEVEL_INFO, "Proxy test OK");
    if (ui->rNoProxy->isChecked())
    {
        preferences->setProxyType(Preferences::PROXY_TYPE_NONE);
    }
    else if (ui->rProxyAuto->isChecked())
    {
        preferences->setProxyType(Preferences::PROXY_TYPE_AUTO);
    }
    else if (ui->rProxyManual->isChecked())
    {
        preferences->setProxyType(Preferences::PROXY_TYPE_CUSTOM);
    }

    preferences->setProxyProtocol(ui->cProxyType->currentIndex());
    preferences->setProxyServer(ui->eProxyServer->text().trimmed());
    preferences->setProxyPort(ui->eProxyPort->text().toInt());
    preferences->setProxyRequiresAuth(ui->cProxyRequiresPassword->isChecked());
    preferences->setProxyUsername(ui->eProxyUsername->text());
    preferences->setProxyPassword(ui->eProxyPassword->text());

    app->applyProxySettings();   

    if (proxyTestProgressDialog)
    {
        proxyTestProgressDialog->hide();
        delete proxyTestProgressDialog;
        proxyTestProgressDialog = NULL;
    }

    if (shouldClose)
    {
        shouldClose = false;
        this->close();
    }
    else loadSettings();
}

void SettingsDialog::on_bUpdate_clicked()
{
    if (ui->bUpdate->text() == tr("Check for updates"))
    {
        app->checkForUpdates();
    }
    else
    {
        app->triggerInstallUpdate();
    }
}

void SettingsDialog::on_bFullCheck_clicked()
{
    preferences->setCrashed(true);
    QPointer<SettingsDialog> currentDialog = this;
    if (QMessageBox::warning(NULL, tr("Full scan"), tr("MEGAsync will perform a full scan of your synced folders when it starts.\n\nDo you want to restart MEGAsync now?"),
                         QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        if (currentDialog)
        {
            app->rebootApplication(false);
        }
    }
}

void SettingsDialog::onAnimationFinished()
{
    if (ui->wStack->currentWidget() == ui->pAccount)
    {
        ui->pAccount->show();
    }
    else if (ui->wStack->currentWidget() == ui->pSyncs)
    {
        ui->pSyncs->show();
    }
    else if (ui->wStack->currentWidget() == ui->pBandwidth)
    {
        ui->pBandwidth->show();
    }
    else if (ui->wStack->currentWidget() == ui->pProxies)
    {
        ui->pProxies->show();
    }
    else if (ui->wStack->currentWidget() == ui->pAdvanced)
    {
        ui->pAdvanced->show();
    }
}

void SettingsDialog::on_bStorageDetails_clicked()
{
    accountDetailsDialog = new AccountDetailsDialog(megaApi, this);
    app->updateUserStats(true, true, true, true, USERSTATS_STORAGECLICKED);
    QPointer<AccountDetailsDialog> dialog = accountDetailsDialog;
    dialog->exec();
    if (!dialog)
    {
        return;
    }

    delete accountDetailsDialog;
    accountDetailsDialog = NULL;
}

void SettingsDialog::setUpdateAvailable(bool updateAvailable)
{
    if (updateAvailable)
    {
        ui->bUpdate->setText(tr("Install update"));
    }
    else
    {
        ui->bUpdate->setText(tr("Check for updates"));
    }
}

void SettingsDialog::openSettingsTab(int tab)
{
    switch (tab)
    {
    case ACCOUNT_TAB:
        reloadUIpage = true;
        on_bAccount_clicked();
        break;

    case SYNCS_TAB:
        on_bSyncs_clicked();
        break;

    case BANDWIDTH_TAB:
        on_bBandwidth_clicked();
        break;

    case PROXY_TAB:
        on_bProxies_clicked();
        break;

    case ADVANCED_TAB:
        on_bAdvanced_clicked();
        break;

    default:
        break;
    }
}

void SettingsDialog::on_lAccountImage_clicked()
{
    debugCounter++;
    if (debugCounter == 5)
    {
        app->toggleLogging();
        debugCounter = 0;
    }
}

void SettingsDialog::on_bChangePassword_clicked()
{
    QPointer<ChangePassword> cPassword = new ChangePassword(this);
    int result = cPassword->exec();
    if (!cPassword || result != QDialog::Accepted)
    {
        delete cPassword;
        return;
    }

    delete cPassword;
}

MegaProgressDialog::MegaProgressDialog(const QString &labelText, const QString &cancelButtonText,
                                       int minimum, int maximum, QWidget *parent, Qt::WindowFlags f) :
    QProgressDialog(labelText, cancelButtonText, minimum, maximum, parent, f) {}

void MegaProgressDialog::reject() {}
void MegaProgressDialog::closeEvent(QCloseEvent * event)
{
    event->ignore();
}
