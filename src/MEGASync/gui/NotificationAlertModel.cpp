#include "NotificationAlertModel.h"

#include "NotificationAlertTypes.h"
#include "MegaNotificationExt.h"
#include "MegaUserAlertExt.h"

#include "megaapi.h"

QModelIndex NotificationAlertModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }

    return createIndex(row, column, mNotifications.at(row));
}

QModelIndex NotificationAlertModel::parent(const QModelIndex& index) const
{
    Q_UNUSED(index)

    return QModelIndex();
}

int NotificationAlertModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : mNotifications.size();
}

int NotificationAlertModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant NotificationAlertModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0)
    {
        return QVariant();
    }

    if (role == Qt::DisplayRole)
    {
        return QVariant::fromValue(index.internalId());
    }
    else if (role == Qt::UserRole && mNotifications.at(index.row())->getType() == NotificationExtBase::Type::ALERT)
    {
        auto alert = qobject_cast<const MegaUserAlertExt*>(mNotifications.at(index.row()));
        return QDateTime::fromMSecsSinceEpoch(alert->getTimestamp(0) * 1000);
    }

    return QVariant();
}

Qt::ItemFlags NotificationAlertModel::flags(const QModelIndex& index) const
{
    return QAbstractItemModel::flags(index) | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

auto NotificationAlertModel::findAlertById(unsigned int id)
{
    auto it = std::find_if(mNotifications.begin(), mNotifications.end(),
                           [id](const NotificationExtBase* current)
                           {
                               if(current->getType() == NotificationExtBase::Type::ALERT)
                               {
                                   auto alertItem = qobject_cast<const MegaUserAlertExt*>(current);
                                   return alertItem && alertItem->getId() == id;
                               }
                               return false;
                           });
    return it;
}

auto NotificationAlertModel::findNotificationById(int64_t id)
{
    auto it = std::find_if(mNotifications.begin(), mNotifications.end(),
                           [id](const NotificationExtBase* current)
                           {
                               if(current->getType() == NotificationExtBase::Type::NOTIFICATION)
                               {
                                   auto notificationItem = qobject_cast<const MegaNotificationExt*>(current);
                                   return notificationItem && notificationItem->getID() == id;
                               }
                               return false;
                           });
    return it;
}

void NotificationAlertModel::processAlerts(mega::MegaUserAlertList* alerts)
{
    int numAlerts = alerts ? alerts->size() : 0;
    if (numAlerts)
    {
        QList<mega::MegaUserAlert*> newAlerts;
        QList<mega::MegaUserAlert*> updatedAlerts;
        QList<mega::MegaUserAlert*> removedAlerts;
        for (int i = 0; i < numAlerts; i++)
        {
            mega::MegaUserAlert* alert = alerts->get(i);
            auto it = findAlertById(alert->getId());
            if (it == mNotifications.end() && !alert->isRemoved())
            {
                if(alert->isRemoved())
                {
                    removedAlerts.append(alert);
                }
                else
                {
                    newAlerts.append(alert->copy());
                }
            }
            else if (it != mNotifications.end())
            {
                updatedAlerts.append(alert->copy());
            }
        }

        removeAlerts(removedAlerts);
        insertAlerts(newAlerts);
        updateAlerts(updatedAlerts);
    }
}

void NotificationAlertModel::insertAlerts(const QList<mega::MegaUserAlert*>& alerts)
{
    if(alerts.size() <= 0)
    {
        return;
    }

    beginInsertRows(QModelIndex(),
                    mNotifications.size(),
                    mNotifications.size() + alerts.size() - 1);
    for (auto& alert : alerts)
    {
        auto alertItem = new MegaUserAlertExt(alert);
        mNotifications.push_back(alertItem);

        if(!alertItem->isSeen())
        {
            mSeenStatusManager.markAsUnseen(alertItem->getAlertType());
        }
    }
    endInsertRows();
}

void NotificationAlertModel::updateAlerts(const QList<mega::MegaUserAlert*>& alerts)
{
    if(alerts.size() <= 0)
    {
        return;
    }

    for (auto& alert : alerts)
    {
        auto it = findAlertById(alert->getId());
        if (it != mNotifications.end())
        {
            int row = std::distance(mNotifications.begin(), it);
            auto alertItem = qobject_cast<MegaUserAlertExt*>(mNotifications[row]);

            if(alertItem->isSeen() && !alert->getSeen())
            {
                mSeenStatusManager.markAsUnseen(alertItem->getAlertType());
            }
            else if(!alertItem->isSeen() && alert->getSeen())
            {
                mSeenStatusManager.markAsSeen(alertItem->getAlertType());
            }

            alertItem->reset(alert);
            emit dataChanged(index(row, 0, QModelIndex()), index(row, 0, QModelIndex()));
        }
    }
}

void NotificationAlertModel::removeAlerts(const QList<mega::MegaUserAlert*>& alerts)
{
    if(alerts.size() <= 0)
    {
        return;
    }

    for (auto& alert : alerts)
    {
        auto it = findAlertById(alert->getId());
        if (it != mNotifications.end())
        {
            int row = std::distance(mNotifications.begin(), it);
            auto alertItem = qobject_cast<MegaUserAlertExt*>(mNotifications[row]);
            if(!alertItem->isSeen())
            {
                mSeenStatusManager.markAsSeen(alertItem->getAlertType());
            }

            beginRemoveRows(QModelIndex(), row, row);
            delete mNotifications[row];
            mNotifications.erase(it);
            endRemoveRows();
        }
    }
}

bool NotificationAlertModel::hasAlertsOfType(AlertType type)
{
    return std::any_of(mNotifications.begin(), mNotifications.end(),
                       [type](const NotificationExtBase* current)
                       {
                           if(current->getType() == NotificationExtBase::Type::ALERT)
                           {
                               auto alertItem = qobject_cast<const MegaUserAlertExt*>(current);
                               return alertItem && alertItem->getAlertType() == type;
                           }
                           return false;
                       });
}

void NotificationAlertModel::processNotifications(const mega::MegaNotificationList* notifications)
{
    int numNotifications = notifications ? notifications->size() : 0;
    if (numNotifications)
    {
        removeNotifications(notifications);
        insertNotifications(notifications);
    }
}

void NotificationAlertModel::insertNotifications(const mega::MegaNotificationList* notifications)
{
    QList<const mega::MegaNotification*> newNotifications;
    for (int i = 0; i < notifications->size(); i++)
    {
        const mega::MegaNotification* notification = notifications->get(i);
        auto it = findNotificationById(notification->getID());
        if (it == mNotifications.end())
        {
            newNotifications.append(notification->copy());
        }
    }

    if(newNotifications.size() <= 0)
    {
        return;
    }

    beginInsertRows(QModelIndex(), 0, newNotifications.size() - 1);
    for (auto& notification : newNotifications)
    {
        auto item = new MegaNotificationExt(notification);
        mNotifications.push_back(item);

        if(!item->isSeen())
        {
            mSeenStatusManager.markAsUnseen(AlertType::NOTIFICATIONS);
        }
    }
    endInsertRows();
}

void NotificationAlertModel::removeNotifications(const mega::MegaNotificationList* notifications)
{
    for (int row = 0; row < mNotifications.size(); ++row)
    {
        auto item = mNotifications[row];
        if(item->getType() != NotificationExtBase::Type::NOTIFICATION)
        {
            continue;
        }

        unsigned i = 0;
        bool found = false;
        auto notification = qobject_cast<MegaNotificationExt*>(item);
        auto id = notification->getID();
        while (i < notifications->size() && !found)
        {
            found = notifications->get(i)->getID() == id;
            ++i;
        }

        if (!found)
        {
            if(!notification->isSeen())
            {
                mSeenStatusManager.markAsSeen(AlertType::NOTIFICATIONS);
            }

            beginRemoveRows(QModelIndex(), row, row);
            delete mNotifications[row];
            mNotifications.removeAt(row);
            endRemoveRows();

            --row;
        }
    }
}

UnseenNotificationsMap NotificationAlertModel::getUnseenNotifications() const
{
    return mSeenStatusManager.getUnseenNotifications();
}

uint32_t NotificationAlertModel::checkLocalLastSeenNotification()
{
    for (auto& item : mNotifications)
    {
        if(item->getType() != NotificationExtBase::Type::NOTIFICATION)
        {
            continue;
        }

        auto currentLastSeen = mSeenStatusManager.getLastSeenNotification();
        auto notification = qobject_cast<MegaNotificationExt*>(item);
        auto id = notification->getID();
        if(!notification->isSeen() && id > currentLastSeen)
        {
            mSeenStatusManager.setLocalLastSeenNotification(id);
        }
    }
    return mSeenStatusManager.getLocalLastSeenNotification();
}

void NotificationAlertModel::setLastSeenNotification(uint32_t id)
{
    if(mSeenStatusManager.getLastSeenNotification() < id)
    {
        mSeenStatusManager.setLastSeenNotification(id);
        mSeenStatusManager.setLocalLastSeenNotification(id);
        for (auto& item : mNotifications)
        {
            if(item->getType() != NotificationExtBase::Type::NOTIFICATION)
            {
                continue;
            }

            auto notification = qobject_cast<MegaNotificationExt*>(item);
            if(notification->getID() <= id && !notification->isSeen())
            {
                notification->markAsSeen();
                mSeenStatusManager.markAsSeen(AlertType::NOTIFICATIONS);
            }
        }
    }
}
