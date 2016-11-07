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

/*
    ProjectItem.cpp

    Contains items of data supplied by the ProjectTree model.
*/

#include "projectitem.h"

#include <QStringList>

ProjectItem::ProjectItem(const QVector<QVariant> &data, ProjectItem *parent)
{
    parentItem = parent;
    itemData = data;
}

ProjectItem::~ProjectItem()
{
    qDeleteAll(childItems);
}

ProjectItem *ProjectItem::child(int number)
{
    return childItems.value(number);
}

int ProjectItem::childCount() const
{
    return childItems.count();
}

int ProjectItem::childNumber() const
{
    if (parentItem)
    {
        return parentItem->childItems.indexOf(const_cast<ProjectItem*>(this));
    }

    return 0;
}

int ProjectItem::columnCount() const
{
    return itemData.count();
}

QVariant ProjectItem::data(int column) const
{
    return itemData.value(column);
}

bool ProjectItem::insertChildren(int position, int count, int columns)
{
    if ((position < 0) || (position > childItems.size()))
        return false;

    for (int row = 0; row < count; ++row)
    {
        QVector<QVariant> data(columns);
        ProjectItem *item = new ProjectItem(data, this);
        childItems.insert(position, item);
    }

    return true;
}

bool ProjectItem::insertColumns(int position, int columns)
{
    if ((position < 0) || (position > itemData.size()))
        return false;

    for (int column = 0; column < columns; ++column)
    {
        itemData.insert(position, QVariant());
    }

    foreach (ProjectItem *child, childItems)
    {
        child->insertColumns(position, columns);
    }

    return true;
}

ProjectItem *ProjectItem::parent()
{
    return parentItem;
}

bool ProjectItem::removeChildren(int position, int count)
{
    if ((position < 0) || ((position + count) > childItems.size()))
        return false;

    for (int row = 0; row < count; ++row)
    {
        delete childItems.takeAt(position);
    }

    return true;
}

bool ProjectItem::removeColumns(int position, int columns)
{
    if ((position < 0) || ((position + columns) > itemData.size()))
        return false;

    for (int column = 0; column < columns; ++column)
    {
        itemData.remove(position);
    }

    foreach (ProjectItem *child, childItems)
    {
        child->removeColumns(position, columns);
    }

    return true;
}

bool ProjectItem::setData(int column, const QVariant &value)
{
    if ((column < 0) || (column >= itemData.size()))
    {
        return false;
    }

    itemData[column] = value;
    return true;
}
