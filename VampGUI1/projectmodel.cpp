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

#include <QtWidgets>

#include "projectitem.h"
#include "projectmodel.h"

ProjectModel::ProjectModel(const QStringList &headers, QObject *parent)
    : QAbstractItemModel(parent)
{
    QVector<QVariant> rootData;
    foreach (QString header, headers)
    {
        rootData << header;
    }

    rootItem = new ProjectItem(rootData);
/* Not used
    setupModelData(data.split(QString("\n")), rootItem);
*/
}

ProjectModel::~ProjectModel()
{
    delete rootItem;
}

int ProjectModel::columnCount(const QModelIndex & /* parent */) const
{
    return rootItem->columnCount();
}

QVariant ProjectModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DecorationRole)
    {
        ProjectItem *item = getItem(index);

        if ((index.column() == 0) && (item->childCount() > 0))
            return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
        else
            return QVariant();
    }
/*    else
    if (role == Qt::SizeHintRole)
    {
        int col = index.column();
        //QSize val = this->headerData()
        if (index.column() > 0)
            return QSize(100, 20);
        else
            return QSize(500, 20);
    }
*/
    if ((role != Qt::DisplayRole) && (role != Qt::EditRole) && (role != Qt::BackgroundRole))
        return QVariant();

    ProjectItem *item = getItem(index);

    QVariant ret = item->data(index.column());

    if (role == Qt::BackgroundRole)
    {
        QString str = ret.toString();

        if (index.column() == 1)
        {
            if (str == "YES")
                return QColor(Qt::green);
            else
            if (str == "NO")
                return QColor(Qt::red);
            else
            if (str == "OLD")
                return QColor(Qt::yellow);
            else
                return QColor(Qt::white);
        }
        else
        if (index.column() > 1)
        {
            if ((str == "100%") || (str == "N/A"))
                return QColor(Qt::green);
            else
            if (str.right(1) == "%")
            {
                if (str.left(str.length() - 1).toInt() < 50)
                    return QColor(Qt::red);
                else
                    return QColor(Qt::yellow);
            }
        }
        else
            return QColor(Qt::white);
    }

    return ret;
}

Qt::ItemFlags ProjectModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return /*Qt::ItemIsEditable | */QAbstractItemModel::flags(index);
}

ProjectItem *ProjectModel::getItem(const QModelIndex &index) const
{
    if (index.isValid())
    {
        ProjectItem *item = static_cast<ProjectItem*>(index.internalPointer());
        if (item)
            return item;
    }

    return rootItem;
}

QVariant ProjectModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        return rootItem->data(section);

    return QVariant();
}

QModelIndex ProjectModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && (parent.column() != 0))
        return QModelIndex();

    ProjectItem *parentItem = getItem(parent);

    ProjectItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

bool ProjectModel::insertColumns(int position, int columns, const QModelIndex &parent)
{
    bool success;

    beginInsertColumns(parent, position, position + columns - 1);
    success = rootItem->insertColumns(position, columns);
    endInsertColumns();

    return success;
}

bool ProjectModel::insertRows(int position, int rows, const QModelIndex &parent)
{
    ProjectItem *parentItem = getItem(parent);
    bool success;

    beginInsertRows(parent, position, position + rows - 1);
    success = parentItem->insertChildren(position, rows, rootItem->columnCount());
    endInsertRows();

    return success;
}

QModelIndex ProjectModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    ProjectItem *childItem = getItem(index);
    ProjectItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

// FIXME: Probably not needed
bool ProjectModel::removeColumns(int position, int columns, const QModelIndex &parent)
{
    bool success;

    beginRemoveColumns(parent, position, position + columns - 1);
    success = rootItem->removeColumns(position, columns);
    endRemoveColumns();

    if (rootItem->columnCount() == 0)
        removeRows(0, rowCount());

    return success;
}

bool ProjectModel::removeRows(int position, int rows, const QModelIndex &parent)
{
    ProjectItem *parentItem = getItem(parent);
    bool success = true;

    beginRemoveRows(parent, position, position + rows - 1);
    success = parentItem->removeChildren(position, rows);
    endRemoveRows();

    return success;
}

int ProjectModel::rowCount(const QModelIndex &parent) const
{
    ProjectItem *parentItem = getItem(parent);

    return parentItem->childCount();
}

bool ProjectModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole)
        return false;

    ProjectItem *item = getItem(index);
    bool result = item->setData(index.column(), value);

    if (result)
        emit dataChanged(index, index);

    return result;
}

bool ProjectModel::setHeaderData(int section, Qt::Orientation orientation,
                              const QVariant &value, int role)
{
    if ((role != Qt::EditRole) || (orientation != Qt::Horizontal))
        return false;

    bool result = rootItem->setData(section, value);

    if (result)
        emit headerDataChanged(orientation, section, section);

    return result;
}

/* Not used
void ProjectModel::setupModelData(const QStringList &lines, ProjectItem *parent)
{
    QList<ProjectItem*> parents;
    QList<int> indentations;
    parents << parent;
    indentations << 0;

    int number = 0;

    while (number < lines.count())
    {
        int position = 0;
        while (position < lines[number].length())
        {
            if (lines[number].mid(position, 1) != " ")
                break;
            ++position;
        }

        QString lineData = lines[number].mid(position).trimmed();

        if (!lineData.isEmpty())
        {
            // Read the column data from the rest of the line.
            QStringList columnStrings = lineData.split("\t", QString::SkipEmptyParts);
            QVector<QVariant> columnData;
            for (int column = 0; column < columnStrings.count(); ++column)
            {
                columnData << columnStrings[column];
            }

            if (position > indentations.last())
            {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.

                if (parents.last()->childCount() > 0)
                {
                    parents << parents.last()->child(parents.last()->childCount() - 1);
                    indentations << position;
                }
            }
            else
            {
                while ((position < indentations.last()) && (parents.count() > 0))
                {
                    parents.pop_back();
                    indentations.pop_back();
                }
            }

            // Append a new item to the current parent's list of children.
            ProjectItem *parent = parents.last();
            parent->insertChildren(parent->childCount(), 1, rootItem->columnCount());
            for (int column = 0; column < columnData.size(); ++column)
            {
                parent->child(parent->childCount() - 1)->setData(column, columnData[column]);
            }
        }

        ++number;
    }
}
*/
