#include "MegaDownloader.h"
#include "Utilities.h"
#include "MegaApplication.h"
#include <QDateTime>
#include <QPointer>

using namespace mega;

MegaDownloader::MegaDownloader(MegaApi *megaApi) : QObject()
{
    this->megaApi = megaApi;
}

MegaDownloader::~MegaDownloader()
{

}

void MegaDownloader::download(MegaNode *parent, QString path, QString appData)
{
    return download(parent, QFileInfo(path), appData);
}

bool MegaDownloader::processDownloadQueue(QQueue<MegaNode *> *downloadQueue, QString path, unsigned long long appDataId)
{
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(QString::fromLatin1(".")))
    {
        qDeleteAll(*downloadQueue);
        downloadQueue->clear();
        return false;
    }

    TransferMetaData *data = ((MegaApplication*)qApp)->getTransferAppData(appDataId);

    QString currentPath;
    while (!downloadQueue->isEmpty())
    {
        QString appData = QString::number(appDataId);
        MegaNode *node = downloadQueue->dequeue();
        if (node->isForeign() && pathMap.contains(node->getParentHandle()))
        {
            currentPath = pathMap[node->getParentHandle()];
        }
        else
        {
            if (data)
            {
                if (node->isFolder())
                {
                    data->totalFolders++;
                }
                else
                {
                    data->totalFiles++;
                }
                appData.append(QString::fromUtf8("*"));

                if (data->localPath.isEmpty())
                {
                    data->localPath = QDir::toNativeSeparators(path);
                    if (data->totalTransfers == 1)
                    {
                        char *escapedName = megaApi->escapeFsIncompatible(node->getName());
                        QString nodeName = QString::fromUtf8(escapedName);
                        delete [] escapedName;
                        data->localPath += QDir::separator() + nodeName;
                    }
                }
            }

            currentPath = path;
        }

        download(node, currentPath, appData);
        delete node;
    }
    pathMap.clear();
    return true;
}

void MegaDownloader::download(MegaNode *parent, QFileInfo info, QString appData)
{
    QPointer<MegaDownloader> safePointer = this;
    QApplication::processEvents();
    if (!safePointer)
    {
        return;
    }

    QString currentPath = QDir::toNativeSeparators(info.absoluteFilePath());

    if (parent->getType() == MegaNode::TYPE_FILE)
    {
        megaApi->startDownloadWithData(parent, (currentPath + QDir::separator()).toUtf8().constData(),appData.toUtf8().constData());
    }
    else
    {
        if (!parent->isForeign())
        {
            megaApi->startDownloadWithData(parent, (currentPath + QDir::separator()).toUtf8().constData(), appData.toUtf8().constData());
        }
        else
        {
            char *escapedName = megaApi->escapeFsIncompatible(parent->getName());
            QString nodeName = QString::fromUtf8(escapedName);
            delete [] escapedName;

            QString destPath = currentPath + QDir::separator() + nodeName;
            QDir dir(destPath);
            if (!dir.exists())
            {
    #ifndef WIN32
                if (!megaApi->createLocalFolder(dir.toNativeSeparators(destPath).toUtf8().constData()))
    #else
                if (!dir.mkpath(QString::fromLatin1(".")))
    #endif
                {
                    return;
                }
            }

            QByteArray appDataArray = appData.toUtf8();
            char *endptr;
            unsigned long long notificationId = strtoll(appDataArray.constData(), &endptr, 10);
            TransferMetaData *data = ((MegaApplication*)qApp)->getTransferAppData(notificationId);
            if (data)
            {
                if ((endptr - appDataArray.constData()) != appData.size())
                {
                    data->transfersFolderOK++;
                }

                data->pendingTransfers--;
                if (data->pendingTransfers == 0)
                {
                    //Transfers finished, show notification
                    emit finishedTransfers(notificationId);
                }
            }

            pathMap[parent->getHandle()] = destPath;
        }
    }
}
