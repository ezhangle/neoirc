#ifndef ServerDialog_HPP
#define ServerDialog_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <set>
#include <boost/uuid/sha1.hpp>
#include <QtWidgets/QDialog>
#include <QPainter>
#include <QMessageBox>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_hook.hpp>
#include "ui_server.h"

#include "model.hpp"
#include "server.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ServerDialog : public QDialog
		{
			Q_OBJECT

		private:

		public:
			ServerDialog(irc::model& aModel, const irc::server& aServer = irc::server(), QWidget *parent = 0) :
				QDialog(parent), iServer(aServer), iNetworkPixmap(":/irc/Resources/network.png")
			{
				ui.setupUi(this);
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				setWindowTitle(aServer.name().empty() ? "New Server" : "Edit Server");
				std::set<std::string> networks;
				for (irc::server_list::const_iterator i = aModel.server_list().begin(); i != aModel.server_list().end(); ++i)
					networks.insert(i->network());
				for (std::set<std::string>::const_iterator i = networks.begin(); i != networks.end(); ++i)
					ui.comboBox_Network->addItem(iNetworkPixmap, (*i).c_str());
				ui.comboBox_Network->lineEdit()->setText(aServer.network().c_str());
				ui.lineEdit_Name->setText(aServer.name().c_str());
				ui.lineEdit_Address->setText(aServer.address().c_str());
				std::string ports;
				for (irc::server::port_list::const_iterator i = aServer.ports().begin(); i != aServer.ports().end();)
				{
					const irc::server::port_range& port = *i++;
					std::ostringstream text;
					if (port.first == port.second)
						text << port.first;
					else
						text << port.first << "-" << port.second;
					ports += text.str();
					if (i != aServer.ports().end())
						ports += ",";
				}
				if (ports.empty())
					ports = "6667";
				ui.lineEdit_Ports->setText(ports.c_str());
				ui.checkBox_HasConnectionPassword->setChecked(aServer.password());
				ui.checkBox_Secure->setChecked(aServer.secure());
			}

		public:
			void getServer(irc::server& aResult)
			{
				aResult.set_network(ui.comboBox_Network->lineEdit()->text().toStdString());
				aResult.set_name(ui.lineEdit_Name->text().toStdString());
				aResult.set_address(ui.lineEdit_Address->text().toStdString());
				irc::server::port_list portList;
				std::vector<std::string> ports;
				neolib::tokens(ui.lineEdit_Ports->text().toStdString(), std::string(","), ports);
				for (std::vector<std::string>::iterator i = ports.begin(); i != ports.end(); ++i)
				{
					neolib::vecarray<std::string, 2> portRange;
					neolib::tokens(*i, std::string("-"), portRange, 2);
					switch (portRange.size())
					{
					case 0:
						break;
					case 1:
						portList.push_back(irc::server::port_range(
							static_cast<irc::server::port_type>(neolib::string_to_unsigned_integer(portRange[0], 10)),
							static_cast<irc::server::port_type>(neolib::string_to_unsigned_integer(portRange[0], 10))));
						break;
					case 2:
						portList.push_back(irc::server::port_range(
							static_cast<irc::server::port_type>(neolib::string_to_unsigned_integer(portRange[0], 10)),
							static_cast<irc::server::port_type>(neolib::string_to_unsigned_integer(portRange[1], 10))));
						break;
					}
				}
				aResult.set_ports(portList);
				aResult.set_password(ui.checkBox_HasConnectionPassword->isChecked());
				aResult.set_secure(ui.checkBox_Secure->isChecked());
			}

			private slots :
			virtual void done(int result)
			{
				if (result == QDialog::Accepted)
				{
					if (ui.comboBox_Network->lineEdit()->text().isEmpty())
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Please enter a network name", QMessageBox::Ok);
						return;
					}
					if (ui.lineEdit_Name->text().isEmpty())
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Please enter a server name", QMessageBox::Ok);
						return;
					}
					if (ui.lineEdit_Address->text().isEmpty())
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Please enter a server address", QMessageBox::Ok);
						return;
					}
					if (ui.lineEdit_Ports->text().isEmpty())
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Please enter a server port", QMessageBox::Ok);
						return;
					}
				}
				QDialog::done(result);
			}

		private:
			irc::server iServer;
			Ui::server ui;
			QPixmap iNetworkPixmap;
		};
	}
}

#endif // SettingsDialog_HPP
