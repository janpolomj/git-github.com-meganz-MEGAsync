#include "LinuxPlatform.h"
#include <map>

using namespace std;
using namespace mega;

ExtServer *LinuxPlatform::ext_server = NULL;
NotifyServer *LinuxPlatform::notify_server = NULL;

static QString autostart_dir = QDir::homePath() + QString::fromLatin1("/.config/autostart/");
QString LinuxPlatform::desktop_file = autostart_dir + QString::fromLatin1("megasync.desktop");
QString LinuxPlatform::set_icon = QString::fromUtf8("gvfs-set-attribute -t string \"%1\" metadata::custom-icon file://%2");
QString LinuxPlatform::remove_icon = QString::fromUtf8("gvfs-set-attribute -t unset \"%1\" metadata::custom-icon");
QString LinuxPlatform::custom_icon = QString::fromUtf8("/usr/share/icons/hicolor/256x256/apps/mega.png");

void LinuxPlatform::initialize(int argc, char *argv[])
{
}

void LinuxPlatform::prepareForSync()
{

}

bool LinuxPlatform::enableTrayIcon(QString executable)
{
    return false;
}

void LinuxPlatform::notifyItemChange(string *localPath, int)
{
    if (notify_server && localPath && localPath->size()
            && !Preferences::instance()->overlayIconsDisabled())
    {
        notify_server->notifyItemChange(localPath);
    }
}

// enable or disable MEGASync launching at startup
// return true if operation succeeded
bool LinuxPlatform::startOnStartup(bool value)
{
    // copy desktop file into autostart directory
    if (value)
    {
        if (QFile(desktop_file).exists())
        {
            return true;
        }
        else
        {
            // make sure directory exist
            if (!QDir(autostart_dir).exists())
            {
                if (!QDir().mkdir(autostart_dir))
                {
                    //LOG_debug << "Failed to create autostart dir: " << autostart_dir;
                    return false;
                }
            }
            QString app_desktop = QString::fromLatin1("/usr/share/applications/megasync.desktop");
            if (QFile(app_desktop).exists())
            {
                return QFile::copy(app_desktop, desktop_file);
            }
            else
            {
                //LOG_debug << "Desktop file does not exist: " << app_desktop;
                return false;
            }
        }
    }
    else
    {
        // remove desktop file if it exists
        if (QFile(desktop_file).exists())
        {
            return QFile::remove(desktop_file);
        }
    }
    return true;
}

bool LinuxPlatform::isStartOnStartupActive()
{
    return QFile(desktop_file).exists();
}

void LinuxPlatform::showInFolder(QString pathIn)
{
    QString filebrowser = getDefaultFileBrowserApp();
    QProcess::startDetached(filebrowser + QString::fromLatin1(" \"") + pathIn + QString::fromUtf8("\""));
}

void LinuxPlatform::startShellDispatcher(MegaApplication *receiver)
{
    if (!ext_server)
    {
        ext_server = new ExtServer(receiver);
    }

    if (!notify_server)
    {
        notify_server = new NotifyServer();
    }
}

void LinuxPlatform::stopShellDispatcher()
{
    if (ext_server)
    {
        delete ext_server;
        ext_server = NULL;
    }

    if (notify_server)
    {
        delete notify_server;
        notify_server = NULL;
    }
}

void LinuxPlatform::syncFolderAdded(QString syncPath, QString syncName, QString syncID)
{
    if (QFile(custom_icon).exists())
    {
        QFile *folder = new QFile(syncPath);
        if (folder->exists())
        {
            QProcess::startDetached(set_icon.arg(folder->fileName()).arg(custom_icon));
        }
        delete folder;

    }

    if (notify_server)
    {
        notify_server->notifySyncAdd(syncPath);
    }
}

void LinuxPlatform::syncFolderRemoved(QString syncPath, QString syncName, QString syncID)
{
    QFile *folder = new QFile(syncPath);
    if (folder->exists())
    {
        QProcess::startDetached(remove_icon.arg(folder->fileName()));
    }
    delete folder;

    if (notify_server)
    {
        notify_server->notifySyncDel(syncPath);
    }
}

void LinuxPlatform::notifyRestartSyncFolders()
{

}

void LinuxPlatform::notifyAllSyncFoldersAdded()
{

}

void LinuxPlatform::notifyAllSyncFoldersRemoved()
{

}

QByteArray LinuxPlatform::encrypt(QByteArray data, QByteArray key)
{
    return data;
}

QByteArray LinuxPlatform::decrypt(QByteArray data, QByteArray key)
{
    return data;
}

QByteArray LinuxPlatform::getLocalStorageKey()
{
    return QByteArray(128, 0);
}

QString LinuxPlatform::getDefaultFileBrowserApp()
{
    return getDefaultOpenAppByMimeType(QString::fromUtf8("inode/directory"));
}

QString LinuxPlatform::getDefaultOpenApp(QString extension)
{
    char *mimeType = MegaApi::getMimeType(extension.toUtf8().constData());
    if (!mimeType)
    {
        return QString();
    }
    QString qsMimeType(QString::fromUtf8(mimeType));
    delete mimeType;
    return getDefaultOpenAppByMimeType(qsMimeType);
}

QString LinuxPlatform::getDefaultOpenAppByMimeType(QString mimeType)
{
    QString getDefaultAppDesktopFileName = QString::fromUtf8("xdg-mime query default ") + mimeType;

    QProcess process;
    process.start(getDefaultAppDesktopFileName,
                  QIODevice::ReadWrite | QIODevice::Text);
    if(!process.waitForFinished(5000))
    {
        return QString();
    }

    QString desktopFileName = QString::fromUtf8(process.readAllStandardOutput());
    desktopFileName = desktopFileName.trimmed();
    desktopFileName.replace(QString::fromUtf8(";"), QString::fromUtf8(""));
    if (!desktopFileName.size())
    {
        return QString();
    }

    QFileInfo desktopFile(QString::fromUtf8("/usr/share/applications/") + desktopFileName);
    if (!desktopFile.exists())
    {
        return QString();
    }

    QFile f(desktopFile.absoluteFilePath());
    if (!f.open(QFile::ReadOnly | QFile::Text))
    {
        return QString();
    }

    QTextStream in(&f);
    QStringList contents = in.readAll().split(QString::fromUtf8("\n"));
    contents = contents.filter(QRegExp(QString::fromUtf8("^Exec=")));
    if (!contents.size())
    {
        return QString();
    }

    QString line = contents.at(contents.size() - 1);
    int index = line.indexOf(QChar::fromLatin1('%'));
    int size = -1;
    if (index != -1)
    {
        size = index - 6;
    }

    if (!size)
    {
        return QString();
    }

    return line.mid(5, size);
}

void LinuxPlatform::enableDialogBlur(QDialog *dialog)
{

}

void LinuxPlatform::activateBackgroundWindow(QDialog *)
{

}

void LinuxPlatform::execBackgroundWindow(QDialog *window)
{
    window->exec();
}

bool LinuxPlatform::registerUpdateJob()
{
    return true;
}

void LinuxPlatform::uninstall()
{

}

QStringList LinuxPlatform::getListRunningProcesses()
{
    QProcess p;
    p.start(QString::fromUtf8("ps ax -o comm"));
    p.waitForFinished(2000);
    QString output = QString::fromUtf8(p.readAllStandardOutput().constData());
    QString e = QString::fromUtf8(p.readAllStandardError().constData());
    if (e.size())
    {
        MegaApi::log(MegaApi::LOG_LEVEL_ERROR, "Error for \"ps ax -o comm\" command:");
        MegaApi::log(MegaApi::LOG_LEVEL_ERROR, e.toUtf8().constData());
    }

    p.start(QString::fromUtf8("/bin/bash -c \"readlink /proc/*/exe\""));
    p.waitForFinished(2000);
    QString output2 = QString::fromUtf8(p.readAllStandardOutput().constData());
    e = QString::fromUtf8(p.readAllStandardError().constData());
    if (e.size())
    {
        MegaApi::log(MegaApi::LOG_LEVEL_ERROR, "Error for \"readlink /proc/*/exe\" command:");
        MegaApi::log(MegaApi::LOG_LEVEL_ERROR, e.toUtf8().constData());
    }
    QStringList data = output.split(QString::fromUtf8("\n"));

    data.append(output2.split(QString::fromUtf8("\n")));

    return data;
}

// Check if it's needed to start the local HTTP server
// for communications with the webclient
bool LinuxPlatform::shouldRunHttpServer()
{
    QStringList data = getListRunningProcesses();
    if (data.size() > 1)
    {
        for (int i = 1; i < data.size(); i++)
        {
            // The MEGA webclient sends request to MEGAsync to improve the
            // user experience. We check if web browsers are running because
            // otherwise it isn't needed to run the local web server for this purpose.
            // Here is the list or web browsers that allow HTTP communications
            // with 127.0.0.1 inside HTTPS webs.
            QString command = data.at(i).trimmed();
            if (command.contains(QString::fromUtf8("firefox"), Qt::CaseInsensitive)
                    || command.contains(QString::fromUtf8("chrome"), Qt::CaseInsensitive)
                    || command.contains(QString::fromUtf8("chromium"), Qt::CaseInsensitive)
                    )
            {
                return true;
            }
        }
    }
    return false;
}

// Check if it's needed to start the local HTTPS server
// for communications with the webclient
bool LinuxPlatform::shouldRunHttpsServer()
{
    QStringList data = getListRunningProcesses();

    if (data.size() > 1)
    {
        for (int i = 1; i < data.size(); i++)
        {
            // The MEGA webclient sends request to MEGAsync to improve the
            // user experience. We check if web browsers are running because
            // otherwise it isn't needed to run the local web server for this purpose.
            // Here is the list or web browsers that don't allow HTTP communications
            // with 127.0.0.1 inside HTTPS webs and therefore require a HTTPS server.
            QString command = data.at(i).trimmed();
            if (command.contains(QString::fromUtf8("safari"), Qt::CaseInsensitive)
                    || command.contains(QString::fromUtf8("iexplore"), Qt::CaseInsensitive)
                    || command.contains(QString::fromUtf8("opera"), Qt::CaseInsensitive)
                    || command.contains(QString::fromUtf8("iceweasel"), Qt::CaseInsensitive)
                    || command.contains(QString::fromUtf8("konqueror"), Qt::CaseInsensitive)
                    )
            {
                return true;
            }
        }
    }
    return false;
}

bool LinuxPlatform::isUserActive()
{
    return true;
}
