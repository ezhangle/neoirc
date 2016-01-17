#ifndef ServerTreeModel_HPP
#define ServerTreeModel_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <set>
#include <boost/filesystem.hpp>
#include <QAbstractListModel>
#include <QPixmap>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <neolib/i_application.hpp>
#include <neolib/variant.hpp>
#include <neolib/tree.hpp>
#include <neolib/i_settings.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_utils.hpp>
#include <caw/qt/qt_hook.hpp>

#include "model.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ServerTreeModel : public QAbstractItemModel
		{
		public:
			typedef std::set<std::string> network_list;
			typedef neolib::variant<network_list::iterator, irc::server_list::iterator> element_type;
			typedef neolib::tree<element_type> tree_type;
		public:
			ServerTreeModel(tree_type& aItemTree, QTreeView& aControl) : iItemTree(aItemTree), iControl(aControl),
				iNetworkPixmap(":/irc/Resources/network.png"),
				iServerPixmap(":/irc/Resources/server.png")
			{
			}
		public:
			void start_refresh()
			{
				beginRemoveRows(index(0, 0, QModelIndex()), 0, rowCount());
			}
			void end_refresh()
			{
				endRemoveRows();
				beginResetModel();
				endResetModel();
			}
		public:
			virtual QVariant data(const QModelIndex &index, int role) const
			{
				switch (role)
				{
				case Qt::DisplayRole:
				{
					tree_type::iterator nodeIter = iItemTree.to_iterator(index.internalPointer());
					if (nodeIter->is<network_list::iterator>())
						return QString(static_variant_cast<const network_list::iterator&>(*nodeIter)->c_str());
					else if (nodeIter->is<irc::server_list::iterator>())
						return QString(static_variant_cast<const irc::server_list::iterator&>(*nodeIter)->name().c_str());
					else
						return QString();
				}
					break;
				case Qt::DecorationRole:
				{
					tree_type::iterator nodeIter = iItemTree.to_iterator(index.internalPointer());
					if (nodeIter->is<network_list::iterator>())
						return iNetworkPixmap;
					else
						return iServerPixmap;
				}
					break;
				}
				return QVariant();
			}
			virtual Qt::ItemFlags flags(const QModelIndex &index) const
			{
				if (!index.isValid())
					return 0;
				return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			}
			virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const
			{
				return QVariant();
			}
			virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const
			{
				if (!hasIndex(row, column, parent))
					return QModelIndex();

				tree_type::value_type* parentItem;

				if (!parent.isValid())
				{
					if (row >= iItemTree.count_children(iItemTree.root()))
						return QModelIndex();
					return createIndex(row, column, iItemTree.to_node_ptr(std::next(iItemTree.sibling_begin(), row)));
				}
				else
				{
					tree_type::iterator parentIter = iItemTree.to_iterator(parent.internalPointer());
					if (parentIter->is<irc::server_list::iterator>())
						return QModelIndex();
					if (row >= iItemTree.count_children(parentIter))
						return QModelIndex();
					return createIndex(row, column, iItemTree.to_node_ptr(std::next(iItemTree.sibling_begin(parentIter), row)));
				}
			}
			virtual QModelIndex parent(const QModelIndex &index) const
			{
				if (!index.isValid())
					return QModelIndex();

				tree_type::sibling_iterator iterNode = iItemTree.to_iterator(index.internalPointer());
				tree_type::sibling_iterator iterParent = iItemTree.parent(iterNode);

				if (iterParent == iItemTree.root())
					return QModelIndex();

				return createIndex(std::distance(iItemTree.sibling_begin(iItemTree.parent(iterParent)), iterParent), 0, iItemTree.to_node_ptr(iterParent));
			}
			virtual int rowCount(const QModelIndex &parent = QModelIndex()) const
			{
				if (parent.column() > 0)
					return 0;

				tree_type::iterator parentNode;
				if (!parent.isValid())
					parentNode = iItemTree.root();
				else
				{
					parentNode = iItemTree.to_iterator(parent.internalPointer());
					if (parentNode->is<irc::server_list::iterator>())
						return 0;
				}
				return iItemTree.count_children(parentNode);
			}
			virtual int columnCount(const QModelIndex &parent = QModelIndex()) const
			{
				return 1;
			}
		private:
			tree_type& iItemTree;
			QTreeView& iControl;
			QPixmap iNetworkPixmap;
			QPixmap iServerPixmap;
		};
	}
}

#endif // ServerTreeModel_HPP
