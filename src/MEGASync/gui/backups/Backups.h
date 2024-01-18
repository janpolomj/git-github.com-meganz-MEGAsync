#ifndef BACKUPS_H
#define BACKUPS_H

#include "qml/QmlDialogWrapper.h"

#include <QScreen>

class Backups : public QMLComponent
{
    Q_OBJECT

public:

    explicit Backups(QObject *parent = 0);

    QUrl getQmlUrl() override;

    QString contextName() override;

    static void registerQmlModules();

    Q_INVOKABLE void openBackupsTabInPreferences() const;

};

#endif // BACKUPS_H
