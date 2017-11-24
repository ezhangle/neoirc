#ifndef ManageAutoJoinsDialog_HPP
#define ManageAutoJoinsDialog_HPP

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
#include "ui_manageautojoins.h"

#include "model.hpp"
#include "identity.hpp"
#include "auto_joins.hpp"

#include "AutoJoinDialog.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ManageAutoJoinsDialog : public QDialog, private irc::auto_joins_observer
		{
			Q_OBJECT

		private:
			typedef neolib::variant<irc::identities::container_type::iterator, irc::server_key, irc::auto_joins::container_type::iterator> element_type;
			typedef neolib::tree<element_type> tree_type;
			class TreeModel : public QAbstractItemModel
			{
			public:
				TreeModel(tree_type& aItemTree, QTreeView& aControl) : iItemTree(aItemTree), iControl(aControl),
					iIdentityPixmap(":/irc/Resources/user.png"),
					iNetworkPixmap(":/irc/Resources/network.png"),
					iServerPixmap(":/irc/Resources/server.png"),
					iChannelPixmap(":/irc/Resources/channel.png"),
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
					for (int i = 0; i < 4; ++i)
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
						else if (nodeIter->is<irc::server_key>())
						{
							const irc::server_key& server = static_variant_cast<const irc::server_key&>(*nodeIter);
							switch (index.column())
							{
							case 1:
								return QString(server.first.c_str());
							case 2:
								return QString(server.second != "*" ? server.second.c_str() : "* (Any)");
							}
						}
						else if (nodeIter->is<irc::auto_joins::container_type::iterator>())
						{
							const irc::auto_joins::container_type::iterator& iterAutoJoin = static_variant_cast<const irc::auto_joins::container_type::iterator&>(*nodeIter);
							switch (index.column())
							{
							case 3:
								return QString(iterAutoJoin->channel().c_str());
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
						else if (nodeIter->is<irc::server_key>())
						{
							switch (index.column())
							{
							case 1:
								return iNetworkPixmap;
							case 2:
								return iServerPixmap;
							}
						}
						else if (nodeIter->is<irc::auto_joins::container_type::iterator>() && index.column() == 3)
						{
							return iChannelPixmap;
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
						case 3:
							return QString("Channel");
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
						if (parentIter->is<irc::auto_joins::container_type::iterator>())
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
						if (parentNode->is<irc::auto_joins::container_type::iterator>())
							return 0;
					}
					return iItemTree.count_children(parentNode);
				}
				virtual int columnCount(const QModelIndex &parent = QModelIndex()) const
				{
					return 4;
				}
			private:
				tree_type& iItemTree;
				QTreeView& iControl;
				QPixmap iIdentityPixmap;
				QPixmap iNetworkPixmap;
				QPixmap iServerPixmap;
				QPixmap iChannelPixmap;
				mutable std::map<void*, QModelIndex> iIndexMap;
				std::size_t iRefreshing;
			};

		public:
			ManageAutoJoinsDialog(neolib::i_application& aApplication, neolib::i_settings& aSettings, irc::model& aModel, QWidget *parent = 0) :
				QDialog(parent), iApplication(aApplication), iSettings(aSettings), iModel(aModel)
			{
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				ui.setupUi(this);
				ui.treeView_AutoJoins->setModel(new TreeModel(iAutoJoinTree, *ui.treeView_AutoJoins));
				connect(ui.treeView_AutoJoins->selectionModel(), &QItemSelectionModel::currentChanged, [this](){ updateControls(); });
				connect(ui.treeView_AutoJoins, &QAbstractItemView::doubleClicked, [this]() { editItem(); });
				connect(ui.pushButton_New, &QPushButton::clicked, [this]()
				{
					AutoJoinDialog dialog(iModel, irc::auto_join(), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::auto_join autoJoin;
						dialog.getEntry(autoJoin);
						iModel.auto_joins().entries().push_back(autoJoin);
						irc::write_auto_joins(iModel);
						populateTree();
						selectItem(autoJoin);
					}
				});
				connect(ui.pushButton_Edit, &QPushButton::clicked, [this]() { editItem(); });
				connect(ui.pushButton_Remove, &QPushButton::clicked, [this]() { removeItem(); });
				connect(ui.pushButton_MoveUp, &QPushButton::clicked, [this]() { moveItemUp(); });
				connect(ui.pushButton_MoveDown, &QPushButton::clicked, [this]() { moveItemDown(); });
				ui.checkBox_AutoConnectOnStartup->setChecked(iSettings.find_setting("Connection", "AutoConnectOnStartup").value().value_as_boolean());
				connect(ui.checkBox_AutoConnectOnStartup, &QPushButton::clicked, [this]()
				{
					iSettings.change_setting(iSettings.find_setting("Connection", "AutoConnectOnStartup"), ui.checkBox_AutoConnectOnStartup->isChecked(), true);
				});
				populateTree();
				updateControls();
				iModel.auto_joins().add_observer(*this);
			}
			~ManageAutoJoinsDialog()
			{
				iModel.auto_joins().remove_observer(*this);
			}

		public:

			private slots :
				virtual void done(int result)
			{
				QDialog::done(result);
			}
		private:
			// from irc::auto_joins_observer
			virtual void auto_join_added(const irc::auto_join& aEntry)
			{
			}
			virtual void auto_join_updated(const irc::auto_join& aEntry)
			{
			}
			virtual void auto_join_removed(const irc::auto_join& aEntry)
			{
			}
			virtual void auto_join_cleared()
			{
			}
			virtual void auto_join_reset()
			{
			}

			void editItem()
			{
				if (ui.treeView_AutoJoins->currentIndex() == QModelIndex())
					return;
				tree_type::iterator iterNode = iAutoJoinTree.to_iterator(ui.treeView_AutoJoins->currentIndex().internalPointer());
				if (iterNode->is<irc::auto_joins::container_type::iterator>())
				{
					AutoJoinDialog dialog(iModel, *static_variant_cast<irc::auto_joins::container_type::iterator>(*iterNode), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::auto_join& autoJoin = *static_variant_cast<irc::auto_joins::container_type::iterator>(*iterNode);
						dialog.getEntry(autoJoin);
						irc::write_auto_joins(iModel);
						populateTree();
						selectItem(autoJoin);
					}
				}
			}
			void removeItem()
			{
				if (ui.treeView_AutoJoins->currentIndex() == QModelIndex())
					return;
				tree_type::iterator iterNode = iAutoJoinTree.to_iterator(ui.treeView_AutoJoins->currentIndex().internalPointer());
				if (iterNode->is<irc::auto_joins::container_type::iterator>())
				{
					iModel.auto_joins().remove(*static_variant_cast<irc::auto_joins::container_type::iterator>(*iterNode));
					irc::write_auto_joins(iModel);
					populateTree();
				}
			}
			void moveItemUp()
			{
				QModelIndex currentIndex = ui.treeView_AutoJoins->currentIndex();
				if (currentIndex == QModelIndex())
					return;
				tree_type::sibling_iterator iterNode = iAutoJoinTree.to_iterator(currentIndex.internalPointer());
				if (iterNode != iAutoJoinTree.sibling_begin(iAutoJoinTree.parent(iterNode)))
				{
					static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->start_refresh();
					iAutoJoinTree.swap(iterNode, std::prev(iterNode));
					std::ptrdiff_t offset = std::distance(iAutoJoinTree.begin(), tree_type::iterator(iterNode));
					persistTree();
					static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->end_refresh();
					ui.treeView_AutoJoins->setCurrentIndex(static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->index(std::next(iAutoJoinTree.begin(), offset)));
				}
			}
			void moveItemDown()
			{
				QModelIndex currentIndex = ui.treeView_AutoJoins->currentIndex();
				if (currentIndex == QModelIndex())
					return;
				tree_type::sibling_iterator iterNode = iAutoJoinTree.to_iterator(currentIndex.internalPointer());
				std::ptrdiff_t offset = std::distance(iAutoJoinTree.begin(), tree_type::iterator(iterNode));
				if (iterNode != std::prev(iAutoJoinTree.sibling_end(iAutoJoinTree.parent(iterNode))))
				{
					static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->start_refresh();
					iAutoJoinTree.swap(iterNode, std::next(iterNode));
					std::ptrdiff_t offset = std::distance(iAutoJoinTree.begin(), tree_type::iterator(iterNode));
					persistTree();
					static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->end_refresh();
					ui.treeView_AutoJoins->setCurrentIndex(static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->index(std::next(iAutoJoinTree.begin(), offset)));
				}
			}
			void persistTree()
			{
				std::vector<irc::auto_join> newOrder;
				newOrder.reserve(iModel.auto_joins().entries().size());
				for (tree_type::const_iterator iterNode = iAutoJoinTree.begin(); iterNode != iAutoJoinTree.end(); ++iterNode)
				{
					if (iterNode->is<irc::auto_joins::container_type::iterator>())
						newOrder.push_back(*static_variant_cast<const irc::auto_joins::container_type::iterator&>(*iterNode));
				}
				iModel.auto_joins().clear();
				for (std::size_t i = 0; i < newOrder.size(); ++i)
					iModel.auto_joins().add(newOrder[i]);
				irc::write_auto_joins(iModel);
				populateTree();
			}
			void populateTree()
			{
				static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->start_refresh();
				iAutoJoinTree.clear();
				std::map<std::string, tree_type::iterator> identityNodes;
				std::map<std::pair<std::string, irc::server_key>, tree_type::iterator> serverNodes;
				for (irc::auto_joins::container_type::iterator i = iModel.auto_joins().entries().begin(); i != iModel.auto_joins().entries().end(); ++i)
				{
					if (identityNodes.find(i->nick_name()) == identityNodes.end())
					{
						irc::identities::container_type::iterator iterIdentity = iModel.identities().find_item(i->nick_name());
						if (iterIdentity == iModel.identities().identity_list().end())
							continue;
						identityNodes[i->nick_name()] = iAutoJoinTree.insert(iAutoJoinTree.end(), iterIdentity);
					}
					if (serverNodes.find(std::make_pair(i->nick_name(), i->server())) == serverNodes.end())
					{
						serverNodes[std::make_pair(i->nick_name(), i->server())] = iAutoJoinTree.append(identityNodes[i->nick_name()], i->server());
					}
					iAutoJoinTree.push_back(serverNodes[std::make_pair(i->nick_name(), i->server())], i);
				}
				static_cast<TreeModel*>(ui.treeView_AutoJoins->model())->end_refresh();
			}
			void updateControls()
			{
				bool autoJoinSelected = false;
				if (ui.treeView_AutoJoins->currentIndex() != QModelIndex())
				{
					tree_type::iterator iterNode = iAutoJoinTree.to_iterator(ui.treeView_AutoJoins->currentIndex().internalPointer());
					if (iterNode->is<irc::auto_joins::container_type::iterator>())
						autoJoinSelected = true;
					ui.pushButton_MoveUp->setEnabled(iterNode != iAutoJoinTree.sibling_begin(iAutoJoinTree.parent(iterNode)));
					ui.pushButton_MoveDown->setEnabled(iterNode != std::prev(iAutoJoinTree.sibling_end(iAutoJoinTree.parent(iterNode))));
				}
				ui.pushButton_Edit->setEnabled(autoJoinSelected);
				ui.pushButton_Remove->setEnabled(autoJoinSelected);
			}
			void selectItem(const irc::auto_join& aAutoJoin)
			{
			}

		private:
			neolib::i_application& iApplication;
			neolib::i_settings& iSettings;
			irc::model& iModel;
			Ui::dialog_ManageAutoJoins ui;
			tree_type iAutoJoinTree;
		};
	}
}

#endif // ManageAutoJoinsDialog_HPP
