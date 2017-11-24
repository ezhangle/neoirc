#include <neolib/neolib.hpp>
#include <irc/client/identity.hpp>
#include <irc/client/connection_manager.hpp>
#include "ConnectToServerDialog.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		ConnectToServerDialog::ConnectToServerDialog(neolib::i_application& aApplication, neolib::i_settings& aSettings, irc::model& aModel, QWidget *parent) :
			QDialog(parent), iApplication(aApplication), iModel(aModel),
			iIdentityPixmap(":/irc/Resources/user.png")
		{
			setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
			iModel.server_list_updater().add_observer(*this);
			ui.setupUi(this);
			ui.treeView_Servers->setModel(new ServerTreeModel(iServerTree, *ui.treeView_Servers));
			ui.treeView_Servers->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
			connect(ui.treeView_Servers->selectionModel(), &QItemSelectionModel::currentChanged, [this](){ updateControls(); });
			connect(ui.treeView_Servers, &QAbstractItemView::doubleClicked, [this]()
			{ 
				accept();
			});
			connect(ui.pushButton_NewServer, &QPushButton::clicked, [this]()
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
			connect(ui.comboBox_Identity, (void (QComboBox::*)(int))&QComboBox::currentIndexChanged, [this]()
			{
				identityChanged();
			});
			for (irc::identities::container_type::const_iterator i = iModel.identities().identity_list().begin(); i != iModel.identities().identity_list().end(); ++i)
			{
				ui.comboBox_Identity->addItem(iIdentityPixmap, (*i).nick_name().c_str());
				if (iModel.default_identity().nick_name() == (*i).nick_name())
					ui.comboBox_Identity->setCurrentIndex(ui.comboBox_Identity->count() - 1);
			} 
			populateTree();
			updateControls();
			identityChanged();
		}
		ConnectToServerDialog::~ConnectToServerDialog()
		{
			iModel.server_list_updater().remove_observer(*this);
		}

		void ConnectToServerDialog::done(int result)
		{
			if (result == QDialog::Accepted)
			{
				connectItem();
			}
			QDialog::done(result);
		}

		void ConnectToServerDialog::server_list_downloading()
		{
		}
		void ConnectToServerDialog::server_list_downloaded()
		{
			populateTree();
		}
		void ConnectToServerDialog::server_list_download_failure()
		{
			populateTree();
		}
		void ConnectToServerDialog::selectItem(tree_type::const_iterator aItem)
		{
			for (int i = 0; i < ui.treeView_Servers->model()->rowCount(); ++i)
			{
				for (int j = 0; j < ui.treeView_Servers->model()->rowCount(ui.treeView_Servers->model()->index(i, 0)); ++j)
				{
					QModelIndex index = ui.treeView_Servers->model()->index(j, 0, ui.treeView_Servers->model()->index(i, 0));
					tree_type::const_iterator iterNode = iServerTree.to_iterator(index.internalPointer());
					if (iterNode == aItem)
					{
						ui.treeView_Servers->setCurrentIndex(index);
						ui.treeView_Servers->scrollTo(index);
						return;
					}
				}
			}
		}
		void ConnectToServerDialog::selectItem(const irc::server::key_type& aServerKey)
		{
			for (tree_type::const_iterator i = iServerTree.begin(); i != iServerTree.end(); ++i)
			{
				if (i->is<irc::server_list::iterator>() && *static_variant_cast<irc::server_list::iterator>(*i) == aServerKey)
				{
					selectItem(i);
					return;
				}
			}
		}
		void ConnectToServerDialog::connectItem()
		{
			if (ui.treeView_Servers->currentIndex() == QModelIndex())
				return;
			irc::identities::container_type::const_iterator identity = iModel.identities().find_item(ui.comboBox_Identity->itemText(ui.comboBox_Identity->currentIndex()).toStdString());
			tree_type::iterator iterNode = iServerTree.to_iterator(ui.treeView_Servers->currentIndex().internalPointer());
			if (iterNode->is<irc::server_list::iterator>())
			{
				iModel.connection_manager().add_connection(*static_variant_cast<irc::server_list::iterator>(*iterNode), *identity, std::string(), true);
			}
			else
			{
				std::string network = *static_variant_cast<network_list::iterator>(*iterNode);
				std::vector<irc::server_list::const_iterator> networkServers;
				for (irc::server_list::iterator i = iModel.server_list().begin(); i != iModel.server_list().end(); ++i)
				{
					if ((*i).network() == network)
						networkServers.push_back(i);
				}
				iModel.connection_manager().add_connection(*networkServers[iModel.random().get(networkServers.size() - 1)], *identity, std::string(), true);
			}
		}
		void ConnectToServerDialog::populateTree()
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
		void ConnectToServerDialog::updateControls()
		{
			ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ui.treeView_Servers->currentIndex() != QModelIndex());
		}
		void ConnectToServerDialog::identityChanged()
		{
			bool itemSelected = false;
			irc::identities::container_type::const_iterator identity = iModel.identities().find_item(ui.comboBox_Identity->itemText(ui.comboBox_Identity->currentIndex()).toStdString());
			if (identity != iModel.identity_list().end())
			{
				for (tree_type::const_iterator i = iServerTree.begin(); i != iServerTree.end(); ++i)
				{
					if (i->is<network_list::iterator>() && *static_variant_cast<network_list::iterator>(*i) == identity->last_server_used().first ||
						i->is<irc::server_list::iterator>() && *static_variant_cast<irc::server_list::iterator>(*i) == identity->last_server_used())
					{
						selectItem(i);
						itemSelected = true;
					}
				}
			}
			if (!itemSelected)
			{
				ui.treeView_Servers->clearSelection();
				ui.treeView_Servers->setCurrentIndex(QModelIndex());
				ui.treeView_Servers->scrollToTop();
			}
		}
	}
}
