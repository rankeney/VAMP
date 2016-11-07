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

#include "path.h"
#ifdef USE_QT
#include <QDir>
#endif

Path::Path()
{
}

bool Path::makePath(string path)
{
#ifdef USE_QT
    QString qpath = QString::fromStdString(path);
    QDir dir = QDir::root();
    return dir.mkpath(qpath);
#else
    Make a path here using mkpath or do it the hard way
    return false;
#endif
}

bool Path::comparePath(QString path1, QString path2)
{
#ifdef _WIN32
    // Make sure we have similar path names
    path1.replace("\\", "/");
    path2.replace("\\", "/");
#endif
    // Make sure they both end with a '/'
    if (path1.right(1) != "/")
        path1 += "/";
    if (path2.right(1) != "/")
        path2 += "/";

    return (path1 == path2);
}
