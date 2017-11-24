#ifndef ManageConnectionScriptsDialog_HPP
#define ManageConnectionScriptsDialog_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <set>
#include <boost/filesystem.hpp>
#include <QtWidgets/QDialog>
#include <QPainter>
#include <QMessageBox>
#include <QAbstractListModel>
#include <QPixmap>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <neolib/i_application.hpp>
#include <neolib/i_settings.hpp>
#include <neolib/variant.hpp>
#include <neolib/tree.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_utils.hpp>
#include <caw/qt/qt_hook.hpp>
#include "ui_manageconnectionscripts.h"

#include "model.hpp"
#include "identity.hpp"
#include "connection_script.hpp"

#include "ConnectionScriptDialog.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ManageConnectionScriptsDialog : public QDialog, private irc::connection_scripts_observer
		{
			Q_OBJECT

		private:
			typedef neolib::variant<irc::identities::container_type::iterator, irc::connection_scripts::container_type::iterator> element_type;
			typedef neolib::tree<element_type> tree_type;
			class TreeModel : public QAbstractItemModel
			{
			public:
				TreeModel(tree_type& aItemTree, QTreeView& aControl) : iItemTree(aItemTree), iControl(aControl),
					iIdentityPixmap(":/irc/Resources/user.png"),
					iNetworkPixmap(":/irc/Resources/network.png"),
					iServerPixmap(":/irc/Resources/server.png"),
					iRefreshing(0)
				{
				}
			public:
				void start_refresh()
				{
					if (++iRefreshing > 1)
						return;
					beginRemoveRows(index(0, 0, QModelIndex()), 0, rowCount());
					iIndexMap.clear();
				}
				void end_refresh()
				{
					if (--iRefreshing > 0)
						return;
					endRemoveRows();
					beginResetModel();
					endResetModel();
					iControl.expandAll();
					for (int i = 0; i < columnCount(); ++i)
						iControl.resizeColumnToContents(i);
				}
				QModelIndexList getPersistentIndexList()
				{
					return persistentIndexList();
				}
			public:
				virtual QVariant data(const QModelIndex &index, int role) const
				{
					switch (role)
					{
					case Qt::DisplayRole:
					{
						tree_type::iterator nodeIter = iItemTree.to_iterator(index.internalPointer());
						if (nodeIter->is<irc::identities::container_type::iterator>() && index.column() == 0)
							return QString(static_variant_cast<const irc::identities::container_type::iterator&>(*nodeIter)->nick_name().c_str());
						else if (nodeIter->is<irc::connection_scripts::container_type::iterator>())
						{
							const irc::server_key& server = static_variant_cast<const irc::connection_scripts::container_type::iterator&>(*nodeIter)->server();
							switch (index.column())
							{
							case 1:
								return QString(server.first.c_str());
							case 2:
								return QString(server.second != "*" ? server.second.c_str() : "* (Any)");
							}
						}
						else
							return QVariant();
					}
						break;
					case Qt::DecorationRole:
					{
						tree_type::iterator nodeIter = iItemTree.to_iterator(index.internalPointer());
						if (nodeIter->is<irc::identities::container_type::iterator>() && index.column() == 0)
							return iIdentityPixmap;
						else if (nodeIter->is<irc::connection_scripts::container_type::iterator>())
						{
							switch (index.column())
							{
							case 1:
								return iNetworkPixmap;
							case 2:
								return iServerPixmap;
							}
						}
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
					if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
					{
						switch (section)
						{
						case 0:
							return QString("Identity");
						case 1:
							return QString("Network");
						case 2:
							return QString("Server");
						}
					}
					return QVariant();
				}
				virtual QModelIndex index(tree_type::iterator treeIterator)
				{
					return iIndexMap[iItemTree.to_node_ptr(treeIterator)];
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
						void* nodePointer = iItemTree.to_node_ptr(std::next(iItemTree.sibling_begin(), row));
						QModelIndex index = createIndex(row, column, nodePointer);
						iIndexMap[nodePointer] = index;
						return index;
					}
					else
					{
						tree_type::iterator parentIter = iItemTree.to_iterator(parent.internalPointer());
						if (parentIter->is<irc::connection_scripts::container_type::iterator>())
							return QModelIndex();
						if (row >= iItemTree.count_children(parentIter))
							return QModelIndex();
						void* nodePointer = iItemTree.to_node_ptr(std::next(iItemTree.sibling_begin(parentIter), row));
						QModelIndex index = createIndex(row, column, nodePointer);
						iIndexMap[nodePointer] = index;
						return index;
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
						if (parentNode->is<irc::connection_scripts::container_type::iterator>())
							return 0;
					}
					return iItemTree.count_children(parentNode);
				}
				virtual int columnCount(const QModelIndex &parent = QModelIndex()) const
				{
					return 3;
				}
			private:
				tree_type& iItemTree;
				QTreeView& iControl;
				QPixmap iIdentityPixmap;
				QPixmap iNetworkPixmap;
				QPixmap iServerPixmap;
				mutable std::map<void*, QModelIndex> iIndexMap;
				std::size_t iRefreshing;
			};

		public:
			ManageConnectionScriptsDialog(neolib::i_application& aApplication, neolib::i_settings& aSettings, irc::model& aModel, QWidget *parent = 0) :
				QDialog(parent), iApplication(aApplication), iSettings(aSettings), iModel(aModel)
			{
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				ui.setupUi(this);
				ui.treeView_ConnectionScripts->setModel(new TreeModel(iConnectionScriptTree, *ui.treeView_ConnectionScripts));
				connect(ui.treeView_ConnectionScripts->selectionModel(), &QItemSelectionModel::currentChanged, [this](){ updateControls(); });
				connect(ui.treeView_ConnectionScripts, &QAbstractItemView::doubleClicked, [this]() { editItem(); });
				connect(ui.pushButton_New, &QPushButton::clicked, [this]()
				{
					ConnectionScriptDialog dialog(iModel, irc::connection_script(), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::connection_script connectionScript;
						dialog.getEntry(connectionScript);
						iModel.connection_scripts().entries().push_back(connectionScript);
						irc::write_connection_scripts(iModel);
						populateTree();
						selectItem(connectionScript);
					}
				});
				connect(ui.pushButton_Edit, &QPushButton::clicked, [this]() { editItem(); });
				connect(ui.pushButton_Remove, &QPushButton::clicked, [this]() { removeItem(); });
				populateTree();
				updateControls();
				iModel.connection_scripts().add_observer(*this);
			}
			~ManageConnectionScriptsDialog()
			{
				iModel.connection_scripts().remove_observer(*this);
			}

		public:

			private slots :
				virtual void done(int result)
			{
				QDialog::done(result);
			}
		private:
			// from irc::connection_scripts_observer
			virtual void connection_script_added(const irc::connection_script& aEntry)
			{
			}
			virtual void connection_script_updated(const irc::connection_script& aEntry)
			{
			}
			virtual void connection_script_removed(const irc::connection_script& aEntry)
			{
			}
			virtual void connection_script_cleared()
			{
			}
			virtual void connection_script_reset()
			{
			}

			void editItem()
			{
				if (ui.treeView_ConnectionScripts->currentIndex() == QModelIndex())
					return;
				tree_type::iterator iterNode = iConnectionScriptTree.to_iterator(ui.treeView_ConnectionScripts->currentIndex().internalPointer());
				if (iterNode->is<irc::connection_scripts::container_type::iterator>())
				{
					ConnectionScriptDialog dialog(iModel, *static_variant_cast<irc::connection_scripts::container_type::iterator>(*iterNode), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::connection_script& connectionScript = *static_variant_cast<irc::connection_scripts::container_type::iterator>(*iterNode);
						dialog.getEntry(connectionScript);
						irc::write_connection_scripts(iModel);
						populateTree();
						selectItem(connectionScript);
					}
				}
			}
			void removeItem()
			{
				if (ui.treeView_ConnectionScripts->currentIndex() == QModelIndex())
					return;
				tree_type::iterator iterNode = iConnectionScriptTree.to_iterator(ui.treeView_ConnectionScripts->currentIndex().internalPointer());
				if (iterNode->is<irc::connection_scripts::container_type::iterator>())
				{
					iModel.connection_scripts().remove(*static_variant_cast<irc::connection_scripts::container_type::iterator>(*iterNode));
					irc::write_connection_scripts(iModel);
					populateTree();
				}
			}
			void populateTree()
			{
				static_cast<TreeModel*>(ui.treeView_ConnectionScripts->model())->start_refresh();
				iConnectionScriptTree.clear();
				std::map<std::string, tree_type::iterator> identityNodes;
				for (irc::connection_scripts::container_type::iterator i = iModel.connection_scripts().entries().begin(); i != iModel.connection_scripts().entries().end(); ++i)
				{
					if (identityNodes.find(i->nick_name()) == identityNodes.end())
					{
						irc::identities::container_type::iterator iterIdentity = iModel.identities().find_item(i->nick_name());
						if (iterIdentity == iModel.identities().identity_list().end())
							continue;
						identityNodes[i->nick_name()] = iConnectionScriptTree.insert(iConnectionScriptTree.end(), iterIdentity);
					}
					iConnectionScriptTree.append(identityNodes[i->nick_name()], i);
				}
				static_cast<TreeModel*>(ui.treeView_ConnectionScripts->model())->end_refresh();
			}
			void updateControls()
			{
				bool connectionScriptSelected = false;
				if (ui.treeView_ConnectionScripts->currentIndex() != QModelIndex())
				{
					tree_type::iterator iterNode = iConnectionScriptTree.to_iterator(ui.treeView_ConnectionScripts->currentIndex().internalPointer());
					if (iterNode->is<irc::connection_scripts::container_type::iterator>())
						connectionScriptSelected = true;
				}
				ui.pushButton_Edit->setEnabled(connectionScriptSelected);
				ui.pushButton_Remove->setEnabled(connectionScriptSelected);
			}
			void selectItem(const irc::connection_script& aConnectionScript)
			{
			}

		private:
			neolib::i_application& iApplication;
			neolib::i_settings& iSettings;
			irc::model& iModel;
			Ui::dialog_ManageConnectionScripts ui;
			tree_type iConnectionScriptTree;
		};
	}
}

#endif // ManageConnectionScriptsDialog_HPP
