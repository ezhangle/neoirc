#ifndef AutoJoinDialog_HPP
#define AutoJoinDialog_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <set>
#include <boost/uuid/sha1.hpp>
#include <QtWidgets/QDialog>
#include <QPainter>
#include <QMessageBox>
#include <QComboBox>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_hook.hpp>
#include "ui_autojoin.h"

#include "model.hpp"
#include "auto_joins.hpp"
#include "connection_manager.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class AutoJoinDialog : public QDialog
		{
			Q_OBJECT

		private:

		public:
			AutoJoinDialog(irc::model& aModel, const irc::auto_join& aEntry, QWidget *parent = 0) :
				QDialog(parent), iModel(aModel),
				iNetworkPixmap(":/irc/Resources/network.png"),
				iServerPixmap(":/irc/Resources/server.png"),
				iIdentityPixmap(":/irc/Resources/user.png")
			{
				ui.setupUi(this);
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				if (aEntry != irc::auto_join())
				{
					setWindowTitle("Edit Channel Auto-Join");
					init(aEntry);
				}
				else
				{
					irc::auto_join entry;
					irc::server_key lastServerUsed;
					if (iModel.connection_manager().active_buffer() != 0)
					{
						lastServerUsed = iModel.connection_manager().active_buffer()->connection().server().key();
						entry.set_nick_name(iModel.connection_manager().active_buffer()->connection().identity().nick_name());
					}
					else
					{
						lastServerUsed = iModel.default_identity().last_server_used();
						entry.set_nick_name(iModel.default_identity().nick_name());
					}
					entry.set_server(irc::server_key(lastServerUsed.first, "*"));
					init(entry);
				}
			}

			void init(const irc::auto_join& aEntry)
			{
				connect(ui.comboBox_Network, (void (QComboBox::*)(int))&QComboBox::currentIndexChanged, [this](){
					updateControls();
				});
				ui.comboBox_Network->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
				for (irc::identities::container_type::const_iterator i = iModel.identities().identity_list().begin(); i != iModel.identities().identity_list().end(); ++i)
				{
					ui.comboBox_Identity->addItem(iIdentityPixmap, (*i).nick_name().c_str());
					if (aEntry.nick_name() == (*i).nick_name())
						ui.comboBox_Identity->setCurrentIndex(ui.comboBox_Identity->count() - 1);
				}
				std::set<neolib::ci_string> networks;
				for (irc::server_list::const_iterator i = iModel.server_list().begin(); i != iModel.server_list().end(); ++i)
					networks.insert(neolib::make_ci_string(i->network()));
				for (std::set<neolib::ci_string>::const_iterator i = networks.begin(); i != networks.end(); ++i)
				{
					ui.comboBox_Network->addItem(iNetworkPixmap, (*i).c_str());
					if (aEntry.server().first == *i)
						ui.comboBox_Network->setCurrentIndex(ui.comboBox_Network->count() - 1);
				}
				for (int i = 0; i < ui.comboBox_Server->count(); ++i)
					if (ui.comboBox_Server->itemText(i).toStdString() == aEntry.server().second)
					{
					ui.comboBox_Server->setCurrentIndex(i);
					break;
					}
				ui.lineEdit_Channel->setText(aEntry.channel().c_str());
			}

		public:
			void getEntry(irc::auto_join& aResult)
			{
				aResult.set_nick_name(ui.comboBox_Identity->itemText(ui.comboBox_Identity->currentIndex()).toStdString());
				aResult.set_server(irc::server_key(
					ui.comboBox_Network->itemText(ui.comboBox_Network->currentIndex()).toStdString(),
					ui.comboBox_Server->itemText(ui.comboBox_Server->currentIndex()).toStdString()));
				if (aResult.server().second[0] == '*')
					aResult.set_server(irc::server_key(aResult.server().first, "*"));
				aResult.set_channel(ui.lineEdit_Channel->text().toStdString());
			}

		private slots :
			virtual void done(int result)
			{
				if (result == QDialog::Accepted)
				{

				}
				QDialog::done(result);
			}
		private:
			void updateControls()
			{
				ui.comboBox_Server->clear();
				if (ui.comboBox_Network->currentIndex() != -1)
				{
					std::string network = ui.comboBox_Network->currentText().toStdString();
					for (irc::server_list::const_iterator i = iModel.server_list().begin(); i != iModel.server_list().end(); ++i)
						if (i->network() == network)
							ui.comboBox_Server->addItem(iServerPixmap, i->name().c_str());
					ui.comboBox_Server->addItem(iServerPixmap, "* (Any)");
					ui.comboBox_Server->setCurrentIndex(ui.comboBox_Server->count() - 1);
				}
			}

		private:
			irc::model& iModel;
			Ui::dialog_AutoJoin ui;
			QPixmap iNetworkPixmap;
			QPixmap iServerPixmap;
			QPixmap iIdentityPixmap;
		};
	}
}

#endif // SettingsDialog_HPP
