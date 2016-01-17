#ifndef ConnectionScriptDialog_HPP
#define ConnectionScriptDialog_HPP

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
#include "ui_connectionscript.h"

#include "model.hpp"
#include "connection_script.hpp"
#include "connection_manager.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ConnectionScriptDialog : public QDialog
		{
			Q_OBJECT

		private:

		public:
			ConnectionScriptDialog(irc::model& aModel, const irc::connection_script& aEntry = irc::connection_script(), QWidget *parent = 0) :
				QDialog(parent), iModel(aModel),
				iNetworkPixmap(":/irc/Resources/network.png"),
				iServerPixmap(":/irc/Resources/server.png"),
				iIdentityPixmap(":/irc/Resources/user.png")
			{
				ui.setupUi(this);
				if (aEntry != irc::connection_script())
				{
					setWindowTitle("Edit IRC Server Connection Script");
					init(aEntry);
				}
				else
				{
					setWindowTitle("New IRC Server Connection Script");
					irc::connection_script entry;
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

			void init(const irc::connection_script& aEntry)
			{
				connect(ui.comboBox_Network, (void (QComboBox::*)(int))&QComboBox::currentIndexChanged, [this](){
					updateControls();
				});
				ui.comboBox_Network->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
				std::set<std::string> identities;
				for (irc::identities::container_type::const_iterator i = iModel.identities().identity_list().begin(); i != iModel.identities().identity_list().end(); ++i)
					identities.insert(i->nick_name());
				for (std::set<std::string>::const_iterator i = identities.begin(); i != identities.end(); ++i)
				{
					ui.comboBox_Identity->addItem(iIdentityPixmap, (*i).c_str());
					if (aEntry.nick_name() == *i)
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
				ui.checkBox_Enabled->setChecked(aEntry.enabled());
				ui.plainTextEdit_Script->setPlainText(aEntry.script().c_str());
			}

		public:
			void getEntry(irc::connection_script& aResult)
			{
				aResult.set_nick_name(ui.comboBox_Identity->itemText(ui.comboBox_Identity->currentIndex()).toStdString());
				aResult.set_server(irc::server_key(
					ui.comboBox_Network->itemText(ui.comboBox_Network->currentIndex()).toStdString(),
					ui.comboBox_Server->itemText(ui.comboBox_Server->currentIndex()).toStdString()));
				if (aResult.server().second[0] == '*')
					aResult.set_server(irc::server_key(aResult.server().first, "*"));
				aResult.set_enabled(ui.checkBox_Enabled->isChecked());
				aResult.set_script(ui.plainTextEdit_Script->toPlainText().toStdString());
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
			Ui::dialog_ConnectionScript ui;
			QPixmap iNetworkPixmap;
			QPixmap iServerPixmap;
			QPixmap iIdentityPixmap;
		};
	}
}

#endif // SettingsDialog_HPP
