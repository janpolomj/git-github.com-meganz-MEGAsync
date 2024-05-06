#ifndef APPSTATSEVENTS_H
#define APPSTATSEVENTS_H

#include <QObject>
#include <QHash>

class AppStatsEvents : public QObject
{
    Q_OBJECT

public:

    // Event IDs sent to servers for statistics purpose.
    // MEGA Desktop App ranges:
    //   [99500, 99599]
    //   [600000, 699999]
    enum EventTypes
    {
        NONE                                            = 0,
        EVENT_1ST_START                                 = 99500,
        EVENT_1ST_SYNC                                  = 99501,
        EVENT_1ST_SYNCED_FILE                           = 99502,
        EVENT_1ST_WEBCLIENT_DL                          = 99503,
        EVENT_UNINSTALL_STATS                           = 99504,
        EVENT_ACC_CREATION_START                        = 99505,
        // (Deprecated)                                    = 99506,
        // (Deprecated)                                    = 99507,
        EVENT_PRO_REDIRECT                              = 99508,
        EVENT_MEM_USAGE                                 = 99509,
        EVENT_UPDATE                                    = 99510,
        EVENT_UPDATE_OK                                 = 99511,
        EVENT_DUP_FINISHED_TRSF                         = 99512,
        // (Deprecated)                                    = 99513,
        // (Deprecated)                                    = 99514,
        // (Deprecated)                                    = 99515,
        // (Deprecated)                                    = 99516,
        // (Deprecated)                                    = 99517,
        EVENT_OVER_STORAGE_DIAL                         = 99518,
        EVENT_OVER_STORAGE_NOTIF                        = 99519,
        EVENT_OVER_STORAGE_MSG                          = 99520,
        EVENT_ALMOST_OVER_STORAGE_MSG                   = 99521,
        EVENT_ALMOST_OVER_STORAGE_NOTIF                 = 99522,
        EVENT_MAIN_DIAL_WHILE_OVER_QUOTA                = 99523,
        EVENT_MAIN_DIAL_WHILE_ALMOST_OVER_QUOTA         = 99524,
        EVENT_RED_LIGHT_USED_STORAGE_MISMATCH           = 99525,
        EVENT_TRSF_OVER_QUOTA_DIAL                      = 99526,
        EVENT_TRSF_OVER_QUOTA_NOTIF                     = 99527,
        EVENT_TRSF_OVER_QUOTA_MSG                       = 99528,
        EVENT_TRSF_ALMOST_OVER_QUOTA_MSG                = 99529,
        EVENT_PAYWALL_NOTIF                             = 99530,
        EVENT_SYNC_ADD_FAIL_API_EACCESS                 = 99531,
        EVENT_TRSF_ALMOST_OVERQUOTA_NOTIF               = 99532,
        EVENT_1ST_BACKUP                                = 99533,
        EVENT_1ST_BACKED_UP_FILE                        = 99534,
        EVENT_SI_NAMECONFLICT_SOLVED_MANUALLY           = 99535,
        EVENT_SI_NAMECONFLICT_SOLVED_AUTOMATICALLY      = 99536,
        EVENT_SI_NAMECONFLICT_SOLVED_SEMI_AUTOMATICALLY = 99537,
        EVENT_SI_LOCALREMOTE_SOLVED_MANUALLY            = 99538,
        EVENT_SI_LOCALREMOTE_SOLVED_AUTOMATICALLY       = 99539,
        EVENT_SI_LOCALREMOTE_SOLVED_SEMI_AUTOMATICALLY  = 99540,
        EVENT_SI_IGNORE_SOLVED_MANUALLY                 = 99541,
        EVENT_SI_STALLED_ISSUE_RECEIVED                 = 99542,
        EVENT_SI_IGNORE_ALL_SYMLINK                     = 99543,
        EVENT_SI_SMART_MODE_FIRST_SELECTED              = 99544,
        EVENT_SI_ADVANCED_MODE_FIRST_SELECTED           = 99545,
        EVENT_SI_CHANGE_TO_SMART_MODE                   = 99546,
        EVENT_SI_CHANGE_TO_ADVANCED_MODE                = 99547,
        EVENT_SI_FINGERPRINT_MISSING_SOLVED_MANUALLY    = 99548,
        // (Stalled issues reserved)                       = 99549,
        // (Stalled issues reserved)                       = 99550,
        // (Stalled issues reserved)                       = 99551,
        // (Stalled issues reserved)                       = 99552,
        // (Stalled issues reserved)                       = 99553,
        // (Stalled issues reserved)                       = 99554,
        // (Stalled issues reserved)                       = 99555,
        // (Stalled issues reserved)                       = 99556,
        // (Stalled issues reserved)                       = 99557,
        // (Stalled issues reserved)                       = 99558,
        // (Stalled issues reserved)                       = 99559,
        // (Stalled issues reserved)                       = 99560,
        // (Stalled issues reserved)                       = 99561,
        // (Stalled issues reserved)                       = 99562,
        // (Stalled issues reserved)                       = 99563,
        // (Stalled issues reserved)                       = 99564,
        // (Stalled issues reserved)                       = 99565,
        EVENT_DAILY_ACTIVE_USER                         = 99566,
        EVENT_MONTHLY_ACTIVE_USER                       = 99567,
        EVENT_LOGIN_CLICKED                             = 99568,
        EVENT_LOGOUT_CLICKED                            = 99569,
        EVENT_TRANSFER_TAB_CLICKED                      = 99570,
        EVENT_NOTIFICATION_TAB_CLICKED                  = 99571,
        EVENT_NOTIFICATION_SETTINGS_CLICKED             = 99572,
        EVENT_UPGRADE_ACCOUNT_CLICKED                   = 99573,
        EVENT_OPEN_TRANSFER_MANAGER_CLICKED             = 99574,
        EVENT_ADD_SYNC_CLICKED                          = 99575,
        EVENT_ADD_BACKUP_CLICKED                        = 99576,
        EVENT_UPLOAD_CLICKED                            = 99577,
        EVENT_AVATAR_CLICKED                            = 99578,
        EVENT_MENU_CLICKED                              = 99579,
        EVENT_MENU_ABOUT_CLICKED                        = 99580,
        EVENT_MENU_CLOUD_DRIVE_CLICKED                  = 99581,
        EVENT_MENU_ADD_SYNC_CLICKED                     = 99582,
        EVENT_MENU_ADD_BACKUP_CLICKED                   = 99583,
        EVENT_MENU_OPEN_LINKS_CLICKED                   = 99584,
        EVENT_MENU_UPLOAD_CLICKED                       = 99585,
        EVENT_MENU_DOWNLOAD_CLICKED                     = 99586,
        EVENT_MENU_STREAM_CLICKED                       = 99587,
        EVENT_MENU_SETTINGS_CLICKED                     = 99588,
        EVENT_MENU_EXIT_CLICKED                         = 99589,
        EVENT_SETTINGS_GENERAL_TAB_CLICKED              = 99590,
        EVENT_SETTINGS_ACCOUNT_TAB_CLICKED              = 99591,
        EVENT_SETTINGS_SYNC_TAB_CLICKED                 = 99592,
        EVENT_SETTINGS_BACKUP_TAB_CLICKED               = 99593,
        EVENT_SETTINGS_SECURITY_TAB_CLICKED             = 99594,
        EVENT_SETTINGS_FOLDERS_TAB_CLICKED              = 99595,
        EVENT_SETTINGS_NETWORK_TAB_CLICKED              = 99596,
        EVENT_SETTINGS_NOTIFICATIONS_TAB_CLICKED        = 99597,
        EVENT_SETTINGS_EXPORT_KEY_CLICKED               = 99598,
        EVENT_SETTINGS_CHANGE_PASSWORD_CLICKED          = 99599,
        EVENT_SETTINGS_REPORT_ISSUE_CLICKED             = 600000
    };
    Q_ENUM(EventTypes)

    static const char* getEventMessage(EventTypes event);

private:
    static QHash<EventTypes, const char*> statsMap;

};

#endif // APPSTATSEVENTS_H
