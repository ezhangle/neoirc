#ifndef InvalidNicknameDialog_HPP
#define InvalidNicknameDialog_HPP

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
#include "ui_invalidnickname.h"

#include "model.hpp"
#include "connection.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class InvalidNicknameDialog : public QDialog, private irc::connection_observer
		{
			Q_OBJECT

		public:
			InvalidNicknameDialog(irc::connection& aConnection, QWidget *parent = 0) :
				QDialog(parent), iConnection(aConnection)
			{
				iConnection.add_observer(*this);
				ui.setupUi(this);
				ui.lineEdit_Identity->setText(aConnection.identity().nick_name().c_str());
				ui.lineEdit_Network->setText(aConnection.server().network().c_str());
				ui.lineEdit_Server->setText(aConnection.server().name().c_str());
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				show();
			}
			~InvalidNicknameDialog()
			{
				iConnection.remove_observer(*this);
			}

		private:
			virtual void connection_connecting(irc::connection& aConnection) {}
			virtual void connection_registered(irc::connection& aConnection) {}
			virtual void buffer_added(irc::buffer& aBuffer) {}
			virtual void buffer_removed(irc::buffer& aBuffer) {}
			virtual void incoming_message(irc::connection& aConnection, const irc::message& aMessage) {}
			virtual void outgoing_message(irc::connection& aConnection, const irc::message& aMessage) {}
			virtual void connection_quitting(irc::connection& aConnection) {}
			virtual void connection_disconnected(irc::connection& aConnection) { delete this; }
			virtual void connection_giveup(irc::connection& aConnection) {}

		private slots :
			virtual void reject()
			{
				QDialog::reject();
				delete this;
			}
			virtual void accept()
			{
				iConnection.set_nick_name(ui.lineEdit_NewNickname->text().toStdString());
				QDialog::accept();
				delete this;
			}

		private:
			irc::connection& iConnection;
			Ui::Dialog_InvalidNickname ui;
		};
	}
}

#endif // InvalidNicknameDialog_HPP
