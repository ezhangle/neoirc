#ifndef ManageServersDialog_HPP
#define ManageServersDialog_HPP

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
#include <neolib/variant.hpp>
#include <neolib/tree.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_utils.hpp>
#include <caw/qt/qt_hook.hpp>

#include "ui_manageservers.h"

#include "model.hpp"
#include "server_updater.hpp"

#include "ServerDialog.hpp"
#include "ManageAutoJoinsDialog.hpp"
#include "ManageConnectionScriptsDialog.hpp"

#include "ServerTreeModel.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ManageServersDialog : public QDialog, private irc::server_list_updater_observer
		{
			Q_OBJECT
		private:
			typedef ServerTreeModel::network_list network_list;
			typedef ServerTreeModel::tree_type tree_type;
		public:
			ManageServersDialog(neolib::i_application& aApplication, neolib::i_settings& aSettings, irc::model& aModel, QWidget *parent = 0) :
				QDialog(parent), iApplication(aApplication), iModel(aModel)
			{
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				iModel.server_list_updater().add_observer(*this);
				ui.setupUi(this);
				ui.treeView_Servers->setModel(new ServerTreeModel(iServerTree, *ui.treeView_Servers));
				connect(ui.pushButton_Reset, &QPushButton::clicked, [this, &aModel]()
				{
					if (QMessageBox::question(this, "IRC Server List", "Reset server list to default?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
						return;
					boost::filesystem::copy_file(boost::filesystem::path(iApplication.info().application_folder().to_std_string() + "/servers.xml"),
						boost::filesystem::path(aModel.root_path() + "servers.xml"), boost::filesystem::copy_option::overwrite_if_exists);
					irc::read_server_list(aModel, []()->bool
					{
						return QMessageBox::warning(NULL, "Configuration File Error", "The server list file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
					});
					aModel.server_list().sort();
					populateTree();
				});
				connect(ui.pushButton_Download, &QPushButton::clicked, [&aModel]()
				{
					aModel.server_list_updater().download();
				});
				connect(ui.treeView_Servers->selectionModel(), &QItemSelectionModel::currentChanged, [this](){ updateControls(); });
				connect(ui.treeView_Servers, &QAbstractItemView::doubleClicked, [this]() { editItem(); });
				connect(ui.pushButton_New, &QPushButton::clicked, [this]()
				{
					ServerDialog dialog(iModel, irc::server(), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::server server;
						dialog.getServer(server);
						iModel.server_list().push_back(server);
						iModel.server_list().sort();
						irc::write_server_list(iModel);
						populateTree();
						selectItem(server.key());
					}
				});
				connect(ui.pushButton_Edit, &QPushButton::clicked, [this]() { editItem(); });
				connect(ui.pushButton_Remove, &QPushButton::clicked, [this]() { removeItem(); });
				connect(ui.pushButton_AutoJoins, &QPushButton::clicked, [this, &aSettings]() {
					ManageAutoJoinsDialog dialog(iApplication, aSettings, iModel, this);
					dialog.exec();
				});
				connect(ui.pushButton_ConnectionScripts, &QPushButton::clicked, [this, &aSettings]() {
					ManageConnectionScriptsDialog dialog(iApplication, aSettings, iModel, this);
					dialog.exec();
				});
				populateTree();
				updateControls();
			}
			~ManageServersDialog()
			{
				iModel.server_list_updater().remove_observer(*this);
			}

		public:

			private slots :
				virtual void done(int result)
			{
				QDialog::done(result);
			}
		private:
			// from irc::server_list_updater_observer
			virtual void server_list_downloading()
			{
				ui.pushButton_Download->setEnabled(false);
			}
			virtual void server_list_downloaded()
			{
				ui.pushButton_Download->setEnabled(true);
				populateTree();
				QMessageBox::information(this, "IRC Server List", "IRC server list downloaded successfully.", QMessageBox::Ok);
			}
			virtual void server_list_download_failure()
			{
				ui.pushButton_Download->setEnabled(true);
				QMessageBox::warning(this, "IRC Server List", "Failed to download IRC server list.", QMessageBox::Ok);
			}

			void editItem()
			{
				if (ui.treeView_Servers->currentIndex() == QModelIndex())
					return;
				tree_type::iterator iterNode = iServerTree.to_iterator(ui.treeView_Servers->currentIndex().internalPointer());
				if (iterNode->is<irc::server_list::iterator>())
				{
					ServerDialog dialog(iModel, *static_variant_cast<irc::server_list::iterator>(*iterNode), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::server& server = *static_variant_cast<irc::server_list::iterator>(*iterNode);
						dialog.getServer(server);
						iModel.server_list().sort();
						irc::write_server_list(iModel);
						populateTree();
						selectItem(server.key());
					}
				}
			}
			void removeItem()
			{
				if (ui.treeView_Servers->currentIndex() == QModelIndex())
					return;
				tree_type::iterator iterNode = iServerTree.to_iterator(ui.treeView_Servers->currentIndex().internalPointer());
				if (iterNode->is<irc::server_list::iterator>())
				{
					iModel.server_list().erase(static_variant_cast<irc::server_list::iterator>(*iterNode));
					iModel.server_list().sort();
					irc::write_server_list(iModel);
					populateTree();
				}
			}
			void selectItem(const irc::server::key_type& aServerKey)
			{
				for (int i = 0; i < ui.treeView_Servers->model()->rowCount(); ++i)
				{
					for (int j = 0; j < ui.treeView_Servers->model()->rowCount(ui.treeView_Servers->model()->index(i, 0)); ++j)
					{
						QModelIndex index = ui.treeView_Servers->model()->index(j, 0, ui.treeView_Servers->model()->index(i, 0));
						tree_type::iterator iterNode = iServerTree.to_iterator(index.internalPointer());
						if (iterNode->is<irc::server_list::iterator>() && static_variant_cast<irc::server_list::iterator>(*iterNode)->key() == aServerKey)
						{
							ui.treeView_Servers->setCurrentIndex(index);
							return;
						}
					}
				}
			}
			void populateTree()
			{
				static_cast<ServerTreeModel*>(ui.treeView_Servers->model())->start_refresh();
				iNetworkList.clear();
				iServerTree.clear();
				std::map<std::string, tree_type::iterator> parentNodes;
				for (irc::server_list::iterator i = iModel.server_list().begin(); i != iModel.server_list().end(); ++i)
				{
					network_list::iterator network = iNetworkList.insert(i->network()).first;
					if (parentNodes.find(*network) == parentNodes.end())
						parentNodes[*network] = iServerTree.insert(iServerTree.end(), network);
					iServerTree.push_back(parentNodes[*network], i);
				}
				static_cast<ServerTreeModel*>(ui.treeView_Servers->model())->end_refresh();
			}
			void updateControls()
			{
				bool serverSelected = false;
				if (ui.treeView_Servers->currentIndex() != QModelIndex())
				{
					tree_type::iterator iterNode = iServerTree.to_iterator(ui.treeView_Servers->currentIndex().internalPointer());
					if (iterNode->is<irc::server_list::iterator>())
						serverSelected = true;
				}
				ui.pushButton_Edit->setEnabled(serverSelected);
				ui.pushButton_Remove->setEnabled(serverSelected);
			}

		private:
			neolib::i_application& iApplication;
			irc::model& iModel;
			Ui::dialog_ManageServers ui;
			network_list iNetworkList;
			tree_type iServerTree;
		};
	}
}

#endif // ManageServersDialog_HPP
