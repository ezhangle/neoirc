#ifndef KickReasonDialog_HPP
#define KickReasonDialog_HPP

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
#include <irc/client/connection.hpp>
#include <irc/client/buffer.hpp>
#include <irc/client/channel_user.hpp>

#include "ui_kickreason.h"

namespace caw_irc_plugin
{
	namespace gui
	{
		class KeyPressEater : public QObject
		{
			Q_OBJECT

		public:
			KeyPressEater(QDialog& Dialog) : iDialog(Dialog) {}

		protected:
			bool eventFilter(QObject *obj, QEvent *event)
			{
				if (event->type() == QEvent::KeyPress) 
				{
					QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
					if (keyEvent->key() == Qt::Key_Return)
					{
						iDialog.accept();
						return true;
					}
				}
				// standard event processing
				return QObject::eventFilter(obj, event);
			}

		private:
			QDialog& iDialog;
		};

		class KickReasonDialog : public QDialog, private irc::connection_observer
		{
			Q_OBJECT

		public:
			KickReasonDialog(irc::buffer& aBuffer, irc::channel_user& aUser, QWidget *parent = 0) :
				QDialog(parent), iBuffer(&aBuffer)
			{
				iBuffer->connection().add_observer(*this);
				ui.setupUi(this);
				ui.lineEdit_Network->setText(aBuffer.connection().server().network().c_str());
				ui.lineEdit_Server->setText(aBuffer.connection().server().name().c_str());
				ui.lineEdit_Channel->setText(aBuffer.name().c_str());
				ui.lineEdit_User->setText(aUser.nick_name().c_str());
				ui.plainTextEdit_KickReason->installEventFilter(new KeyPressEater(*this));
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				show();
			}
			~KickReasonDialog()
			{
				if (iBuffer)
					iBuffer->connection().remove_observer(*this);
			}

		public:
			std::string kick_reason() const
			{
				return ui.plainTextEdit_KickReason->toPlainText().toStdString();
			}

		private:
			virtual void done(int aResult)
			{
				if (iBuffer)
				{
					iBuffer->connection().remove_observer(*this);
					iBuffer = 0;
				}
				QDialog::done(aResult);
			}
		private:
			virtual void connection_connecting(irc::connection& aConnection) {}
			virtual void connection_registered(irc::connection& aConnection) {}
			virtual void buffer_added(irc::buffer& aBuffer) {}
			virtual void buffer_removed(irc::buffer& aBuffer) { if (&aBuffer == iBuffer) reject(); }
			virtual void incoming_message(irc::connection& aConnection, const irc::message& aMessage) {}
			virtual void outgoing_message(irc::connection& aConnection, const irc::message& aMessage) {}
			virtual void connection_quitting(irc::connection& aConnection) {}
			virtual void connection_disconnected(irc::connection& aConnection) { reject(); }
			virtual void connection_giveup(irc::connection& aConnection) {}

		private:
			irc::buffer* iBuffer;
			Ui::Dialog_KickReason ui;
		};
	}
}

#endif // KickReasonDialog_HPP
