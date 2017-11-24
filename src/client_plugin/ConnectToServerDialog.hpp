#ifndef ConnectToServerDialog_HPP
#define ConnectToServerDialog_HPP

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
#include <neolib/i_settings.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_utils.hpp>
#include <caw/qt/qt_hook.hpp>

#include "ui_connecttoserver.h"

#include "model.hpp"
#include "server_updater.hpp"

#include "ServerDialog.hpp"

#include "ServerTreeModel.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ConnectToServerDialog : public QDialog, private irc::server_list_updater_observer
		{
			Q_OBJECT
		private:
			typedef ServerTreeModel::network_list network_list;
			typedef ServerTreeModel::tree_type tree_type;
		public:
			ConnectToServerDialog(neolib::i_application& aApplication, neolib::i_settings& aSettings, irc::model& aModel, QWidget *parent = 0);
			~ConnectToServerDialog();

		private slots :
			virtual void done(int result);
		private:
			// from irc::server_list_updater_observer
			virtual void server_list_downloading();
			virtual void server_list_downloaded();
			virtual void server_list_download_failure();
			void selectItem(tree_type::const_iterator aItem);
			void selectItem(const irc::server::key_type& aServerKey);
			void connectItem();
			void populateTree();
			void updateControls();
			void identityChanged();

		private:
			neolib::i_application& iApplication;
			irc::model& iModel;
			Ui::dialog_ConnectToServer ui;
			network_list iNetworkList;
			tree_type iServerTree;
			QPixmap iIdentityPixmap;
		};
	}
}

#endif // ConnectToServerDialog_HPP
