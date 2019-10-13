#ifndef UTILITIES_H
#define UTILITIES_H

#include <QString>
#include <QHash>
#include <QPixmap>
#include <QDir>

#include <sys/stat.h>

#ifdef __APPLE__
#define MEGA_SET_PERMISSIONS chmod("/Applications/MEGAsync.app/Contents/MacOS/MEGAclient", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); \
                             chmod("/Applications/MEGAsync.app/Contents/MacOS/MEGAupdater", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); \
                             chmod("/Applications/MEGAsync.app/Contents/MacOS/MEGADeprecatedVersion", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); \
                             chmod("/Applications/MEGAsync.app/Contents/PlugIns/MEGAShellExtFinder.appex/Contents/MacOS/MEGAShellExtFinder", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

struct PlanInfo
{
    int amount;
    QString currency;
    unsigned long long gbStorage;
    unsigned long long gbTransfer;
    int level;
};

struct PSA_info
{
    int idPSA;
    QString title;
    QString desc;
    QString urlImage;
    QString textButton;
    QString urlClick;

    PSA_info()
    {
        clear();
    }

    PSA_info(const PSA_info& info)
    {
        idPSA = info.idPSA;
        title = info.title;
        desc = info.desc;
        urlImage = info.urlImage;
        textButton = info.textButton;
        urlClick = info.urlClick;
    }

    void clear()
    {
        idPSA = -1;
        title = QString();
        desc = QString();
        urlImage = QString();
        textButton = QString();
        urlClick = QString();
    }
};

class Utilities
{
public:
    static QString getSizeString(unsigned long long bytes);
    static QString getTimeString(long long secs, bool secondPrecision = true);
    static QString getFinishedTimeString(long long secs);
    static bool verifySyncedFolderLimits(QString path);
    static QString extractJSONString(QString json, QString name);
    static long long extractJSONNumber(QString json, QString name);
    static QString getDefaultBasePath();
    static int percentage(long long val, long long total);

private:
    Utilities() {}
    static QHash<QString, QString> extensionIcons;
    static QHash<QString, QString> languageNames;
    static void initializeExtensions();
    static QString getExtensionPixmap(QString fileName, QString prefix);

//Platform dependent functions
public:
    static QString languageCodeToString(QString code);
    static QString getExtensionPixmapSmall(QString fileName);
    static QString getExtensionPixmapMedium(QString fileName);
    static QString getAvatarPath(QString email);
    static bool removeRecursively(QString path);
    static void copyRecursively(QString srcPath, QString dstPath);
    static void getFolderSize(QString folderPath, long long *size);
    static qreal getDevicePixelRatio();
};

#endif // UTILITIES_H
