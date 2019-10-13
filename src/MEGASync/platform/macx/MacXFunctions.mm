#include "MacXFunctions.h"
#include <Cocoa/Cocoa.h>
#include <QFileInfo>
#include <QCoreApplication>
#include <QWidget>
#include <QProcess>
#include <Preferences.h>

#import <objc/runtime.h>
#import <sys/proc_info.h>
#import <libproc.h>

#include <time.h>
#include <errno.h>
#include <sys/sysctl.h>

#ifndef kCFCoreFoundationVersionNumber10_9
    #define kCFCoreFoundationVersionNumber10_9 855.00
#endif

void setMacXActivationPolicy()
{
    //application does not appear in the Dock
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

QString fromNSString(const NSString *str)
{
    if (!str)
    {
        return QString();
    }

    const char *utf8Str = [str UTF8String];
    if (!utf8Str)
    {
        return QString();
    }
    return QString::fromUtf8(utf8Str);
}

QStringList qt_mac_NSArrayToQStringList(void *nsarray)
{
    QStringList result;
    if (!nsarray)
    {
        return result;
    }

    NSArray *array = static_cast<NSArray *>(nsarray);
    for (NSUInteger i = 0; i < [array count]; ++i)
    {
        QString st;
        if ([[array objectAtIndex:i] isKindOfClass:[NSURL class]])
        {
            st = fromNSString([[array objectAtIndex:i] path]);
        }
        else
        {
            st = fromNSString([array objectAtIndex:i]);
        }
       result.append(st);
    }
    return result;
}

QStringList uploadMultipleFiles(QString uploadTitle)
{
    QStringList uploads;
    static NSOpenPanel *panel = NULL;

    if (!panel)
    {
        panel = [NSOpenPanel openPanel];
        [panel setTitle:[NSString stringWithUTF8String:uploadTitle.toUtf8().constData()]];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:YES];

        NSInteger clicked = [panel runModal];
        if (clicked == NSFileHandlingPanelOKButton)
        {
            uploads = qt_mac_NSArrayToQStringList([panel URLs]);
        }

        panel = NULL;
        return uploads;
    }

    return QStringList();
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
void SetProcessName(QString procname)
{
    CFStringRef process_name = CFStringCreateWithCString(NULL, procname.toUtf8().constData(), kCFStringEncodingUTF8);
    if (!process_name || CFStringGetLength(process_name) == 0)
    {
        if (process_name)
        {
            CFRelease(process_name);
        }
        return;
    }

    if (![NSThread isMainThread])
    {
        CFRelease(process_name);
        return;
    }

    typedef CFTypeRef PrivateLSASN;
    typedef PrivateLSASN (*LSGetCurrentApplicationASNType)();
    typedef OSStatus (*LSSetApplicationInformationItemType)(int, PrivateLSASN,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);

    static LSGetCurrentApplicationASNType ls_get_current_application_asn_func = NULL;
    static LSSetApplicationInformationItemType ls_set_application_information_item_func = NULL;
    static CFStringRef ls_display_name_key = NULL;

    CFStringRef* key_pointer;
    ProcessSerialNumber psn;

    static bool did_symbol_lookup = false;
    if (!did_symbol_lookup)
    {
        did_symbol_lookup = true;
        CFBundleRef launch_services_bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));
        if (!launch_services_bundle)
        {
            CFRelease(process_name);
            return;
        }

        ls_get_current_application_asn_func =
            reinterpret_cast<LSGetCurrentApplicationASNType>(
                CFBundleGetFunctionPointerForName(
                    launch_services_bundle, CFSTR("_LSGetCurrentApplicationASN")));

        ls_set_application_information_item_func =
            reinterpret_cast<LSSetApplicationInformationItemType>(
                CFBundleGetFunctionPointerForName(
                    launch_services_bundle,
                    CFSTR("_LSSetApplicationInformationItem")));

        key_pointer = reinterpret_cast<CFStringRef*>(
                    CFBundleGetDataPointerForName(launch_services_bundle,
                                                  CFSTR("_kLSDisplayNameKey")));

        ls_display_name_key = key_pointer ? *key_pointer : NULL;
        GetCurrentProcess(&psn);
    }

    if (!ls_get_current_application_asn_func
            || !ls_set_application_information_item_func
            || !ls_display_name_key)
    {
        CFRelease(process_name);
        return;
    }

    PrivateLSASN asn = ls_get_current_application_asn_func();
    const int magic_session_constant = -2;
    ls_set_application_information_item_func(magic_session_constant, asn,
                                             ls_display_name_key, process_name, NULL /* optional out param */);
    CFRelease(process_name);
}

char *runWithRootPrivileges(char *command)
{
    if (!command)
    {
        return NULL;
    }

    OSStatus status;
    AuthorizationRef authorizationRef;
    NSString *appPath = [[NSBundle mainBundle] bundlePath];
    if (appPath == nil)
    {
        return NULL;
    }

    NSString *pathToIcon = [appPath stringByAppendingString:@"/Contents/Resources/appicon32.tiff"];
    const char *icon = [pathToIcon fileSystemRepresentation];
    if (!icon)
    {
        return NULL;
    }

    const char *prompt = "MEGAsync. ";
    char *result = NULL;
    FILE *pipe = NULL;
    char* args[3];
    args [0] = "-e";
    args [1] = command;
    args [2] = NULL;

    AuthorizationItem kAuthEnv[] = {
            { kAuthorizationEnvironmentIcon, strlen(icon), (void*)icon, 0 },
            { kAuthorizationEnvironmentPrompt, strlen(prompt), (char *)prompt, 0}};
    AuthorizationEnvironment myAuthorizationEnvironment = { 2, kAuthEnv };
    AuthorizationItem right = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights rights = {1, &right};
    AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed
            | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;

    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                     kAuthorizationFlagDefaults, &authorizationRef);
    if (status != errAuthorizationSuccess)
    {
        return NULL;
    }

    // Call AuthorizationCopyRights to determine rights.
    status = AuthorizationCopyRights(authorizationRef, &rights, &myAuthorizationEnvironment, flags, NULL);
    if (status != errAuthorizationSuccess)
    {
        AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
        return NULL;
    }

    status = AuthorizationExecuteWithPrivileges(authorizationRef, "/usr/bin/osascript",
                                                kAuthorizationFlagDefaults, args, &pipe);
    AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    if (status == errAuthorizationSuccess)
    {
        result = new char[1025];
        memset(result, 0, 1025);
        fread(result, 1024, 1, pipe);
        fclose(pipe);
    }

    return result;
}

bool startAtLogin(bool opt)
{
    NSString *appPath = [[NSBundle mainBundle] bundlePath];
    if (appPath == nil)
    {
        return false;
    }

    CFURLRef url = (CFURLRef)[NSURL fileURLWithPath:appPath];
    if (url == nil)
    {
        return false;
    }

    LSSharedFileListRef loginItems = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
    if (loginItems == nil)
    {
        return false;
    }

    // Remove duplicates
    UInt32 seed = 0U;
    NSArray *items = (NSArray *)LSSharedFileListCopySnapshot(loginItems, &seed);
    if (items)
    {
        for (id item in items)
        {
            LSSharedFileListItemRef itemRef = (LSSharedFileListItemRef)item;
            if (itemRef)
            {
                CFURLRef itemURL = NULL;
                if (LSSharedFileListItemResolve(itemRef, 0, (CFURLRef*) &itemURL, NULL) == noErr && itemURL
                        && CFEqual(url, itemURL))
                {
                    // Remove duplicates with the same target URL
                    LSSharedFileListItemRemove(loginItems, itemRef);
                }
                else
                {
                    CFStringRef itemName = LSSharedFileListItemCopyDisplayName(itemRef);
                    if (itemName)
                    {
                        // Remove duplicates called "MEGAsync"
                        if ([(NSString *)itemName isEqualToString:@"MEGAsync"])
                        {
                            LSSharedFileListItemRemove(loginItems, itemRef);
                        }
                        CFRelease(itemName);
                    }
                }

                if (itemURL)
                {
                    CFRelease(itemURL);
                }
            }
        }
        CFRelease(items);
    }

    bool result = false;
    if (!opt)
    {
        //Disable start at login
        result = true;
    }
    else
    {
        //Enable start at login
        //Insert an item to the login list.
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItems, kLSSharedFileListItemLast,
                                                                     NULL, NULL, url, NULL, NULL);
        if (item)
        {
            CFRelease(item);
            result = true;
        }
    }
    CFRelease(loginItems);
    return result;
}

bool isStartAtLoginActive()
{
    NSString *appPath = [[NSBundle mainBundle] bundlePath];
    if (appPath == nil)
    {
        return false;
    }

    // This will get the path for the application
    NSURL *url = [NSURL fileURLWithPath:appPath];
    if (url == nil)
    {
        return false;
    }

    LSSharedFileListRef loginItems = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
    if (loginItems == nil)
    {
        return false;
    }

    UInt32 seed = 0U;
    Boolean foundIt = false;
    UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;
    NSArray *currentLoginItems = (NSArray *)LSSharedFileListCopySnapshot(loginItems, &seed);
    if (currentLoginItems)
    {
        for (id itemObject in currentLoginItems)
        {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)itemObject;
            if (item)
            {
                CFURLRef itemURL = NULL;
                if (LSSharedFileListItemResolve(item, resolutionFlags, &itemURL, NULL) == noErr && itemURL)
                {
                    foundIt = CFEqual(itemURL, url);
                    CFRelease(itemURL);
                    if (foundIt)
                    {
                        break;
                    }
                }
            }
        }
        CFRelease(currentLoginItems);
    }

    CFRelease(loginItems);
    return (BOOL)foundIt;
}

void addPathToPlaces(QString path, QString pathName)
{
    if (path.isEmpty() || !QFileInfo(path).exists())
    {
        return;
    }

    NSString *appPath = [[NSBundle mainBundle] bundlePath];
    if (!appPath)
    {
        return;
    }

    NSString *folderPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
    if (!folderPath)
    {
        return;
    }

    CFURLRef url = (CFURLRef)[NSURL fileURLWithPath:folderPath];
    if (url == nil)
    {
        return;
    }

    CFURLRef iconURLRef = (CFURLRef)[NSURL fileURLWithPath:[appPath stringByAppendingString:@"/Contents/Resources/app.icns"]];
    if (iconURLRef == nil)
    {
        return;
    }

    FSRef fref;
    if (!CFURLGetFSRef(iconURLRef, &fref))
    {
        return;
    }

    IconRef iconRef;
    if (RegisterIconRefFromFSRef('SSBL', 'ssic', &fref, &iconRef) != noErr)
    {
        return;
    }

    CFStringRef pnString = CFStringCreateWithCString(NULL, pathName.toUtf8().constData(), kCFStringEncodingUTF8);
    if (pnString == nil)
    {
        return;
    }

    // Create a reference to the shared file list.
    LSSharedFileListRef favoriteItems = LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteItems, NULL);
    if (favoriteItems == nil)
    {
        CFRelease(pnString);
        return;
    }

    //Insert an item to the list.
    LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(favoriteItems, kLSSharedFileListItemLast,
                                                                 pnString, iconRef, url, NULL, NULL);
    if (item)
    {
        CFRelease(item);
    }
    CFRelease(favoriteItems);
    CFRelease(pnString);
}

void removePathFromPlaces(QString path)
{
    if (path.isEmpty() || !QFileInfo(path).exists())
    {
        return;
    }

    // Create a reference to the shared file list of favourite items.
    LSSharedFileListRef favoriteItems = LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteItems, NULL);
    if (favoriteItems == nil)
    {
        return;
    }

    NSString *folderPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
    if (!folderPath)
    {
        CFRelease(favoriteItems);
        return;
    }

    // This will get the path for the application
    CFURLRef url = (CFURLRef)[NSURL fileURLWithPath:folderPath];
    if (!url)
    {
        CFRelease(favoriteItems);
        return;
    }

    //Avoid check special volumes (Airdrop)
    UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;

    // loop through the list of startup items and try to find the MEGAsync app
    CFArrayRef listSnapshot = LSSharedFileListCopySnapshot(favoriteItems, NULL);
    if (listSnapshot == nil)
    {
        CFRelease(favoriteItems);
        return;
    }

    for (int i = 0; i < CFArrayGetCount(listSnapshot); i++)
    {
        LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(listSnapshot, i);
        if (item != nil)
        {
            CFURLRef itemURL = NULL;
            if (LSSharedFileListItemResolve(item, resolutionFlags, &itemURL, NULL) == noErr && itemURL)
            {
                if (CFEqual(itemURL, url))
                {
                    LSSharedFileListItemRemove(favoriteItems, item);
                }
                CFRelease(itemURL);
            }
        }
    }

    CFRelease(listSnapshot);
    CFRelease(favoriteItems);
}

void setFolderIcon(QString path)
{
    if (path.isEmpty() || !QFileInfo(path).exists())
    {
        return;
    }

    NSString *appPath = [[NSBundle mainBundle] bundlePath];
    if (!appPath)
    {
        return;
    }

    NSString *folderPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
    if (!folderPath)
    {
        return;
    }

    NSImage* iconImage = NULL;
    if (floor(kCFCoreFoundationVersionNumber) > kCFCoreFoundationVersionNumber10_9)
    {
        iconImage = [[NSImage alloc] initWithContentsOfFile:[appPath stringByAppendingString:@"/Contents/Resources/folder_yosemite.icns"]];
    }
    else
    {
        iconImage = [[NSImage alloc] initWithContentsOfFile:[appPath stringByAppendingString:@"/Contents/Resources/folder.icns"]];
    }

    if (iconImage)
    {
        [[NSWorkspace sharedWorkspace] setIcon:iconImage forFile:folderPath options:0];
        [iconImage release];
    }
}

void unSetFolderIcon(QString path)
{
    if (path.isEmpty() || !QFileInfo(path).exists())
    {
        return;
    }

    NSString *folderPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
    if (!folderPath)
    {
        return;
    }

    [[NSWorkspace sharedWorkspace] setIcon:nil forFile:folderPath options:0];
}

QString defaultOpenApp(QString extension)
{
    CFURLRef appURL = NULL;
    CFStringRef ext;
    CFStringRef info;
    char *buffer;

    ext = CFStringCreateWithCString(NULL, extension.toUtf8().constData(), kCFStringEncodingUTF8);
    if (ext == nil)
    {
        return QString();
    }

    LSGetApplicationForInfo(kLSUnknownType, kLSUnknownCreator, ext, kLSRolesAll, NULL, &appURL);
    if (appURL == NULL)
    {
        CFRelease(ext);
        return QString();
    }
    
    info = CFURLCopyFileSystemPath(appURL, kCFURLPOSIXPathStyle);
    if (info == nil)
    {
        CFRelease(ext);
        CFRelease(appURL);
        return QString();
    }

    CFIndex size = CFStringGetMaximumSizeOfFileSystemRepresentation(info);
    buffer = new char[size];
    CFStringGetCString (info, buffer, size, kCFStringEncodingUTF8);
    QString defaultAppPath = QString::fromUtf8(buffer);
    delete [] buffer;
    CFRelease(info);
    CFRelease(appURL);
    CFRelease(ext);
    return defaultAppPath;
}

void enableBlurForWindow(QWidget *window)
{
    NSView *nsview = (NSView *)window->winId();
    NSWindow *nswindow = [nsview window];

    Class vibrantClass = NSClassFromString(@"NSVisualEffectView");
    if (vibrantClass)
    {
        static const NSRect frameRect = {
            { 0.0, 0.0 },
            { static_cast<float>(window->width()), static_cast<float>(window->height()) }
        };

        auto vibrant = [[vibrantClass alloc] initWithFrame:frameRect];
        [vibrant setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        if ([vibrant respondsToSelector:@selector(setBlendingMode:)])
        {
            [vibrant setBlendingMode:(NSVisualEffectBlendingMode)0];
        }

        //[self addSubview:vibrant positioned:NSWindowBelow relativeTo:nil];
        [nsview addSubview:vibrant positioned:NSWindowBelow relativeTo:nil];
    }
}

bool registerUpdateDaemon()
{
    NSDictionary *plistd = @{
            @"Label": @"mega.mac.megaupdater",
            @"ProgramArguments": @[@"/Applications/MEGAsync.app/Contents/MacOS/MEGAupdater"],
            @"StartInterval": @7200,
            @"RunAtLoad": @true,
            @"StandardErrorPath": @"/dev/null",
            @"StandardOutPath": @"/dev/null",
     };

    const char* home = getenv("HOME");
    if (!home)
    {
        return false;
    }

    NSString *homepath = [NSString stringWithUTF8String:home];
    if (!homepath)
    {
        return false;
    }

    NSString *fullpath = [homepath stringByAppendingString:@"/Library/LaunchAgents/mega.mac.megaupdater.plist"];
    if ([plistd writeToFile:fullpath atomically:YES] == NO)
    {
        return false;
    }

    QString path = QString::fromUtf8([fullpath UTF8String]);
    QFile(path).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);

    QStringList scriptArgs;
    scriptArgs << QString::fromUtf8("-c")
               << QString::fromUtf8("launchctl unload %1 && launchctl load %1").arg(path);

    QProcess p;
    p.start(QString::fromLatin1("bash"), scriptArgs);
    if (!p.waitForFinished(2000))
    {
        return false;
    }

    return p.exitCode();
}

// Check if it's needed to start the local HTTP server
// for communications with the webclient
bool runHttpServer()
{   
    int nProcesses = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    int pidBufSize = nProcesses * sizeof(pid_t);
    pid_t *pids = new pid_t[nProcesses];
    memset(pids, 0, pidBufSize);
    proc_listpids(PROC_ALL_PIDS, 0, pids, pidBufSize);

    for (int i = 0; i < nProcesses; ++i)
    {
        if (pids[i] == 0)
        {
            continue;
        }

        char processPath[PROC_PIDPATHINFO_MAXSIZE];
        memset(processPath, 0, PROC_PIDPATHINFO_MAXSIZE);
        if (proc_pidpath(pids[i], processPath, PROC_PIDPATHINFO_MAXSIZE) <= 0)
        {
            continue;
        }

        int position = strlen(processPath);
        if (position > 0)
        {
            while (position >= 0 && processPath[position] != '/')
            {
                position--;
            }

            // The MEGA webclient sends request to MEGAsync to improve the
            // user experience. We check if web browsers are running because
            // otherwise it isn't needed to run the local web server for this purpose.
            // Here is the list or web browsers that allow HTTP communications
            // with 127.0.0.1 inside HTTPS webs.
            QString processName = QString::fromUtf8(processPath + position + 1);
            if (!processName.compare(QString::fromUtf8("Google Chrome"), Qt::CaseInsensitive)
                || !processName.compare(QString::fromUtf8("firefox"), Qt::CaseInsensitive))
            {
                delete [] pids;
                return true;
            }
        }
    }

    delete [] pids;
    return false;
}

// Check if it's needed to start the local HTTPS server
// for communications with the webclient
bool runHttpsServer()
{
    int nProcesses = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    int pidBufSize = nProcesses * sizeof(pid_t);
    pid_t *pids = new pid_t[nProcesses];
    memset(pids, 0, pidBufSize);
    proc_listpids(PROC_ALL_PIDS, 0, pids, pidBufSize);

    for (int i = 0; i < nProcesses; ++i)
    {
        if (pids[i] == 0)
        {
            continue;
        }

        char processPath[PROC_PIDPATHINFO_MAXSIZE];
        memset(processPath, 0, PROC_PIDPATHINFO_MAXSIZE);
        if (proc_pidpath(pids[i], processPath, PROC_PIDPATHINFO_MAXSIZE) <= 0)
        {
            continue;
        }

        int position = strlen(processPath);
        if (position > 0)
        {
            while (position >= 0 && processPath[position] != '/')
            {
                position--;
            }

            // The MEGA webclient sends request to MEGAsync to improve the
            // user experience. We check if web browsers are running because
            // otherwise it isn't needed to run the local web server for this purpose.
            // Here is the list or web browsers that don't allow HTTP communications
            // with 127.0.0.1 inside HTTPS webs and therefore require a HTTPS server.
            QString processName = QString::fromUtf8(processPath + position + 1);
            if (!processName.compare(QString::fromUtf8("Safari"), Qt::CaseInsensitive)
                || !processName.compare(QString::fromUtf8("Opera"), Qt::CaseInsensitive))
            {
                delete [] pids;
                return true;
            }
        }
    }

    delete [] pids;
    return false;
}

bool userActive()
{
    CFTimeInterval secondsSinceLastEvent = CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGAnyInputEventType);
    if (secondsSinceLastEvent > (Preferences::USER_INACTIVITY_MS / 1000))
    {
         return false;
    }

    return true;
}

double uptime()
{
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if( sysctl(mib, 2, &boottime, &len, NULL, 0) < 0 )
    {
        return -1.0;
    }
    time_t bsec = boottime.tv_sec, csec = time(NULL);

    return difftime(csec, bsec);
}
