/*  Copyright 2016, Robert Ankeney

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/
*/

#include "sysinfo.h"
#ifdef WORKS_SORTA
#include <QSettings>
#endif
#ifdef _WIN32
#include <wbemidl.h>
#include <winreg.h>
#else
#include <dbus/dbus.h>
#endif

SysInfo::SysInfo()
{
}

QString SysInfo::getMachineGuid()
{
#ifdef WORKS_SORTA
    /*
    QSettings settings("HKEY_LOCAL_MACHINE\\Software\\WOW6432node\\Microsoft\\Cryptography",
                        QSettings::NativeFormat);
    QVariant myGuid = settings.value("MachineGuid");
    QSettings settings("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS",
                        QSettings::NativeFormat);
    QVariant myGuid = settings.value("SystemManufacturer");
    */
    QSettings settings("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Cryptography",
                        QSettings::NativeFormat);
    QVariant myGuid = settings.value("MachineGuid");
    return myGuid.toString();
#endif

    QString data;
#ifdef _WIN32
    HKEY hKey;
    long nError;
    nError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey );
    if (nError == ERROR_SUCCESS)
    {
        ULONG nError;
        WCHAR szBuffer[512];
        DWORD dwBufferSize = sizeof(szBuffer);

        nError = RegQueryValueExW(hKey, L"MachineGuid", 0, NULL, (LPBYTE) szBuffer, &dwBufferSize);
        if (ERROR_SUCCESS == nError)
        {
            data = QString::fromWCharArray(szBuffer);
        }

        RegCloseKey(hKey);
    }
#else
    // FIXME - Assumes linux here (not sure about MAC)
    data = QString::fromUtf8(dbus_get_local_machine_id());
#endif
    return data;
}
