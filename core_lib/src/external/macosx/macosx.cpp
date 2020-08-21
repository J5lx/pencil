/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QSysInfo>

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
#include <QOperatingSystemVersion>
#endif

#include "editor.h"
#include "macosxnative.h"
#include "object.h"
#include "pencildef.h"

#include <CoreFoundation/CoreFoundation.h>

namespace PlatformHandler
{
void configurePlatformSpecificSettings()
{
    MacOSXNative::removeUnwantedMenuItems();
}

bool isDarkMode()
{
    return MacOSXNative::isDarkMode();
}

void initialise()
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
}
} // namespace PlatformHandler

extern "C"
{
    // this is not declared in Carbon.h anymore, but it's in the framework
    OSStatus SetMouseCoalescingEnabled(Boolean inNewState, Boolean *outOldState);
}

extern "C"
{
    bool gIsMouseCoalecing = false;

    void detectWhichOSX()
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        QOperatingSystemVersion current = QOperatingSystemVersion::current();
        gIsMouseCoalecing = (current >= QOperatingSystemVersion::OSXElCapitan);
#else
        gIsMouseCoalecing = false;
#endif
    }

    void disableCoalescing()
    {
        SetMouseCoalescingEnabled(gIsMouseCoalecing, NULL);
        // MacOSXNative::setMouseCoalescingEnabled(false);
    }

    void enableCoalescing()
    {
        SetMouseCoalescingEnabled(true, NULL);
        // MacOSXNative::setMouseCoalescingEnabled(true);
    }

} // extern "C"
