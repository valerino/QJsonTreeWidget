/*
    This file is part of QJsonTreeWidget.

    Copyright (C) 2012 valerino <valerio.lupi@te4i.com>

    QJsonTreeWidget is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    QJsonTreeWidget is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with QJsonTreeWidget.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qjsontreemodel.h"

QJsonTreeModel::QJsonTreeModel(QJsonTreeItem* root, QObject *parent) :
  QAbstractItemModel(parent)
{
  m_specialFlags = (QJsonTreeItem::HonorReadOnly | QJsonTreeItem::HonorHide);
  m_root = root;
}

QJsonTreeModel::~QJsonTreeModel()
{
  qDebug() << "~QJsonTreeModel()";
  this->clear();
}


QJsonTreeItem *QJsonTreeModel::parentItem(const QModelIndex &parent) const
{
  if (!parent.isValid())
  {
    // invalid, use root item
    return m_root;
  }
  // get from index
  return static_cast<QJsonTreeItem*>(parent.internalPointer());
}

QModelIndex QJsonTreeModel::index(int row, int column, const QModelIndex &parent) const
{
  // if we haven't an index, just return an empty index
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  // get the parent item
  QJsonTreeItem* parentit = parentItem(parent);
  if (parentit == 0)
    return QModelIndex();

  // since we're referring to a child item of the parent, if it exists create an index for it
  QJsonTreeItem* child = parentit->child(row);
  if (!child)
    return QModelIndex();

  // check various item options
  QVariantMap m = child->map();
  QString tag = child->headerTagByIdx(column);  
  bool b = m.value("__hasROSet__",false).toBool();
  if (b && (m_specialFlags & QJsonTreeItem::ReadOnlyHidesRow) && (m_specialFlags & QJsonTreeItem::HonorHide))
  {
    // hides the whole row
      return QModelIndex();
  }

  b = m.value("_template_",false).toBool();
  if (b)
  {
    // always hide templates
    return QModelIndex();
  }

  b = m.value("_hide_",false).toBool();
  if (b && (m_specialFlags & QJsonTreeItem::HonorHide))
  {
    // hide the whole row
    return QModelIndex();
  }
  b = m.value("_hide_:" % tag,false).toBool();
  if (b && (m_specialFlags & QJsonTreeItem::HonorHide))
  {
    // hide only this value
    return QModelIndex();
  }
  return createIndex (row, column, child);
}

QModelIndex QJsonTreeModel::parent(const QModelIndex &index) const
{
  if (!index.isValid())
    return QModelIndex();

  // look for child parent
  QJsonTreeItem* child = static_cast<QJsonTreeItem*>(index.internalPointer());
   if (child == 0)
     return QModelIndex();

   QJsonTreeItem* parent = child->parent();

   if (parent == m_root)
     return QModelIndex();

   return createIndex (parent->row(), 0, parent);
}

int QJsonTreeModel::columnCount(const QModelIndex &parent) const
{
  if (m_root)
  {
    // return root item's column count (from the _columns_ descriptor)
    return m_root->columnCount();
  }
  return 0;
}

QVariant QJsonTreeModel::data(const QModelIndex &index, int role) const
{
  QJsonTreeItem* item;
  QString tag = tagByModelIndex(index,&item,role);
  if (tag.isEmpty())
    return QVariant();

  // check color
  if (role == Qt::ForegroundRole)
  {
    // read only should be always displayed as disabled
    QString rotag ("_readonly_:" % tag);
    if (item->map().contains(rotag))
      return QVariant();

    if (item->hasChildren())
    {
      // its a parent
      if (!m_parentColor.isValid())
      {
        return QVariant();
      }
      return QVariant(m_parentColor);
    }
    else
    {
      // its a child
      if (!m_childColor.isValid())
      {
        return QVariant();
      }
      return QVariant(m_childColor);
    }
  }

  // skip custom drawn items (they could be buttons, they have usually no values... maybe we could change that later)
  QHash<QString, QHash<QString, QVariant> >  h = item->headers();
  if (h[tag].contains("__draw__"))
    return QVariant();

  // get the value that has been set in json
  return item->map().value(tag,QVariant());
}

bool QJsonTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  QJsonTreeItem* item = itemByModelIndex(index,role);
  if (!item)
    return false;
  if (value.canConvert(QVariant::Map))
  {
    // replace the whole map
    item->fromMap(value.toMap(),item->parent());
  }
  else
  {
    // replace data at the specified JSON tag
    item->setMapValue(index.column(),value);
  }
  emit dataChanged(index,index);

  return true;
}

Qt::ItemFlags QJsonTreeModel::flags(const QModelIndex &index) const
{
  // default
  Qt::ItemFlags f = QAbstractItemModel::flags(index);

  // get tag for this index
  QJsonTreeItem* item;
  QString tag = tagByModelIndex(index,&item);
  if (tag.isEmpty())
    return f;

  // if it's readonly, remove the enabled flag
  bool b = item->map().value("_readonly_:" % tag,false).toBool();
  if (b && (m_specialFlags & QJsonTreeItem::HonorReadOnly))
  {
    f &= ~Qt::ItemIsEnabled;
  }

  // if it's a widget, get flags to apply from the Qt::ItemFlags stored in our hash
  QString w = item->map().value("_widget_:" % tag,QString()).toString();
  if (!w.isEmpty())
  {
    f |= (QJsonTreeItem::widgetFlags[w]);
  }
  return f;
}

QVariant QJsonTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role == Qt::DisplayRole)
    return m_root->headerNameByIdx(section);
  return QVariant();
}

bool QJsonTreeModel::removeRows(int row, int count, const QModelIndex &parent)
{
  QJsonTreeItem* parentit = itemByModelIndex(parent);
  if (!parentit || count == 0)
    return false;

  // this will remove rows starting from the specified row
  beginRemoveRows(parent,row,row+count-1);
  for (int i=row; i < (row+count); i++)
  {
    // this deletes the child too
    parentit->removeChild(i);
  }
  endRemoveRows();
  return true;
}

bool QJsonTreeModel::insertRows(int row, int count, const QModelIndex &parent)
{
  QJsonTreeItem* parentit = itemByModelIndex(parent);
  if (!parentit || count == 0)
    return false;

  beginInsertRows(parent,row,row+count-1);
  for (int i = 0; i < count; i++)
  {
    QJsonTreeItem* newitem = new QJsonTreeItem(parentit->widget(),parentit);
    parentit->appendChild(newitem);
  }
  endInsertRows();

  return true;
}

bool QJsonTreeModel::insertRow(int row, const QModelIndex &parent)
{
  return insertRows(row,1,parent);
}

bool QJsonTreeModel::hasChildren(const QModelIndex &parent)
{
  QJsonTreeItem* item = itemByModelIndex(parent);
  if (item && item->childCount() > 0)
    return true;
  return false;
}

int QJsonTreeModel::rowCount(const QModelIndex &parent) const
{
  // since it's a tree, consider column 0 only
  if (parent.column() > 0)
    return 0;

  QJsonTreeItem* item = parentItem(parent);
  if (item == 0)
    return 0;

  // simply return item children count (= rows under that item)
  return item->childCount();
}

void QJsonTreeModel::clear()
{
  beginResetModel();
  if (m_root)
  {
    delete m_root;
    m_root = 0;
  }
  m_specialFlags = (QJsonTreeItem::HonorReadOnly | QJsonTreeItem::HonorHide);
  endResetModel();
}

void QJsonTreeModel::setSpecialFlags(QJsonTreeItem::SpecialFlags flags)
{
  // special flags is updated, so we need to change the layout and inform the view
  emit layoutAboutToBeChanged();
  m_specialFlags = flags;
  emit layoutChanged();
}

QJsonTreeItem *QJsonTreeModel::itemByModelIndex(const QModelIndex &index, int role) const
{
  // check if index is valid and one of these roles is requested
  if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ForegroundRole && role != Qt::ToolTipRole))
    return 0;

  // get item
  return static_cast<QJsonTreeItem*>(index.internalPointer());
}

QVariantMap QJsonTreeModel::mapByModelIndex(const QModelIndex &index, QJsonTreeItem** item, int role) const
{
  QJsonTreeItem* it = itemByModelIndex(index,role);
  if (!it)
    return QVariantMap();

  if (item)
    *item = it;

  // get item map
  return it->map();
}

const QString QJsonTreeModel::tagByModelIndex (const QModelIndex &index, QJsonTreeItem** item, int role) const
{
  // get item data
  QJsonTreeItem* it;
  QVariantMap m = mapByModelIndex(index,&it,role);
  if (m.isEmpty())
    return QString();
  if (item)
    *item = it;
  return it->headerTagByIdx(index.column());
}

const QModelIndex QJsonTreeModel::indexByItem(QJsonTreeItem* item, int column) const
{
  if (item == m_root)
    return QModelIndex();
  QJsonTreeItem* par = item->parent();
  if (!par)
    par = m_root;
  int row = par->children().lastIndexOf(item);
  return createIndex (row,column,item);
}
