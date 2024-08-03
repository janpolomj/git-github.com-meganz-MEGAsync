DEPENDPATH += $$PWD
INCLUDEPATH += $$PWD

QT       += network

# recreate source folder tree for object files. Needed to build OS utilities,
# otherwise all obj files are placed into same directory, causing overwrite.
CONFIG += object_parallel_to_source

SOURCES += $$PWD/HTTPServer.cpp \
    $$PWD/AccountStatusController.cpp \
    $$PWD/AppStatsEvents.cpp \
    $$PWD/DialogOpener.cpp \
    $$PWD/DownloadQueueController.cpp \
    $$PWD/FileFolderAttributes.cpp \
    $$PWD/LinkObject.cpp \
    $$PWD/LoginController.cpp \
    $$PWD/Preferences/Preferences.cpp \
    $$PWD/Preferences/EphemeralCredentials.cpp \
    $$PWD/Preferences/EncryptedSettings.cpp \
    $$PWD/LinkProcessor.cpp \
    $$PWD/MegaUploader.cpp \
    $$PWD/SetManager.cpp \
    $$PWD/ProxyStatsEventHandler.cpp \
    $$PWD/TransferRemainingTime.cpp \
    $$PWD/UpdateTask.cpp \
    $$PWD/CrashHandler.cpp \
    $$PWD/ExportProcessor.cpp \
    $$PWD/UserAttributesManager.cpp \
    $$PWD/Utilities.cpp \
    $$PWD/ThreadPool.cpp \
    $$PWD/MegaDownloader.cpp \
    $$PWD/MegaSyncLogger.cpp \
    $$PWD/ConnectivityChecker.cpp \
    $$PWD/TransferBatch.cpp \
    $$PWD/TextDecorator.cpp \
    $$PWD/EmailRequester.cpp \
    $$PWD/qrcodegen.c

HEADERS  +=  $$PWD/HTTPServer.h \
    $$PWD/AccountStatusController.h \
    $$PWD/AppStatsEvents.h \
    $$PWD/AsyncHandler.h \
    $$PWD/DialogOpener.h \
    $$PWD/FileFolderAttributes.h \
    $$PWD/DownloadQueueController.h \
    $$PWD/LinkObject.h \
    $$PWD/LoginController.h \
    $$PWD/Preferences/Preferences.h \
    $$PWD/Preferences/EphemeralCredentials.h \
    $$PWD/Preferences/EncryptedSettings.h \
    $$PWD/FileFolderAttributes.h \
    $$PWD/LinkProcessor.h \
    $$PWD/MegaUploader.h \
    $$PWD/ProtectedQueue.h \
    $$PWD/ProxyStatsEventHandler.h \
    $$PWD/SetManager.h \
    $$PWD/SetTypes.h \
    $$PWD/TransferRemainingTime.h \
    $$PWD/UpdateTask.h \
    $$PWD/CrashHandler.h \
    $$PWD/ExportProcessor.h \
    $$PWD/UserAttributesManager.h \
    $$PWD/Utilities.h \
    $$PWD/ThreadPool.h \
    $$PWD/MegaDownloader.h \
    $$PWD/MegaSyncLogger.h \
    $$PWD/ConnectivityChecker.h \
    $$PWD/TransferBatch.h \
    $$PWD/TextDecorator.h \
    $$PWD/Version.h \
    $$PWD/EmailRequester.h \
    $$PWD/qrcodegen.h \
    $$PWD/gzjoin.h
