#include "MegaUploader.h"
#include "control/Utilities.h"
#include "MegaApplication.h"

#include <QMessageBox>
#include <QtCore>
#include <QApplication>
#include <QPointer>
#include <QFile>


#if QT_VERSION >= 0x050000
#include <QtConcurrent/QtConcurrent>
#endif

#ifndef WIN32
#include <utime.h>
#endif

using namespace mega;
using namespace std;


MegaUploader::MegaUploader(MegaApi *megaApi)
{
    this->megaApi = megaApi;
}

MegaUploader::~MegaUploader()
{
}

void MegaUploader::upload(QString path, const QString& nodeName, std::shared_ptr<MegaNode> parent, unsigned long long appDataID, const std::shared_ptr<TransferBatch>& transferBatch)
{
    QFileInfo info(path);

    if (info.isFile() || info.isDir())
    {
        QString currentPath = QDir::toNativeSeparators(info.absoluteFilePath());
        QString msg = QString::fromLatin1("Starting upload : '%1' - '%2' - '%3'").arg(info.fileName(), currentPath).arg(appDataID);
        megaApi->log(MegaApi::LOG_LEVEL_DEBUG, msg.toUtf8().constData());
        transferBatch->add(info.absoluteFilePath());
        startUpload(currentPath, nodeName, appDataID, parent.get(), transferBatch->getCancelTokenPtr());

        emit startingTransfers();
    }
}

void MegaUploader::startUpload(const QString& localPath, const QString &nodeName, unsigned long long appDataID, MegaNode* parent, MegaCancelToken* cancelToken)
{
    const bool startFirst = false;
    QByteArray localPathArray = localPath.toUtf8();

    const char* fileName = nullptr;
    QByteArray fileNameArray;
    if(!nodeName.isEmpty())
    {
        fileNameArray = nodeName.toUtf8();
        fileName = fileNameArray.constData();
    }

    QByteArray appData = (QString::number(appDataID) + QString::fromUtf8("*")).toUtf8();
    const int64_t mtime = ::mega::MegaApi::INVALID_CUSTOM_MOD_TIME;
    const bool isSrcTemporary = false;
    MegaTransferListener* listener = nullptr;
    megaApi->startUpload(localPathArray.constData(), parent, fileName, mtime, appData.constData(), isSrcTemporary, startFirst, cancelToken, listener);
}
