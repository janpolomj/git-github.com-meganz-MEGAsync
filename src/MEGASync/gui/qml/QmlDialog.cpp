#include "QmlDialog.h"

#include <QEvent>

QmlDialog::QmlDialog(QWindow* parent):
    QQuickWindow(parent),
    mIconSrc(QString::fromUtf8(":/images/app_ico.ico")), // Default mega app icon
    mInstancesManager(new QmlInstancesManager())
{
    setFlags(flags() | Qt::Dialog);
    connect(this,
            &QmlDialog::requestPageFocus,
            this,
            &QmlDialog::onRequestPageFocus,
            Qt::QueuedConnection);

    connect(mInstancesManager,
            &QmlInstancesManager::instancesChanged,
            this,
            &QmlDialog::instancesManagerChanged);
}

void QmlDialog::setIconSrc(const QString& iconSrc)
{
    QString source = iconSrc;
    if (iconSrc.startsWith(QString::fromUtf8("qrc:")))
    {
        source = source.mid(3);
    }

    if (source != mIconSrc)
    {
        mIconSrc = source;
        setIcon(QIcon(mIconSrc));
    }
}

QmlInstancesManager* QmlDialog::getInstancesManager()
{
    return mInstancesManager;
}

bool QmlDialog::event(QEvent* event)
{
    if (event->type() == QEvent::Close)
    {
        emit finished();
    }

    return QQuickWindow::event(event);
}

void QmlDialog::onRequestPageFocus()
{
    emit initializePageFocus();
}
