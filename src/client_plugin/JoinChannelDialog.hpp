#ifndef JoinChannelDialog_HPP
#define JoinChannelDialog_HPP

#include <neolib/neolib.hpp>
#include <QtWidgets/QDialog>
#include <caw/i_gui.hpp>
#include "model.hpp"
#include "connection_manager.hpp"
#include "connection.hpp"
#include "ChannelListDialog.hpp"
#include "ui_joinchannel.h"

namespace caw_irc_plugin
{
	namespace gui
	{
		class JoinChannelDialog : public QDialog, private irc::connection_observer
		{
			Q_OBJECT

		public:
			JoinChannelDialog(caw::i_gui& aGui, irc::model& aModel, QWidget *parent = 0) :
				QDialog(parent == 0 ? QApplication::activeWindow() : parent), iGui(aGui), iConnection(aModel.connection_manager().active_buffer()->connection()), iChannelListDialog(0)
			{
				iConnection.add_observer(*this);
				ui.setupUi(this);
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);

				connect(ui.lineEdit_Channel, &QLineEdit::textChanged, [this]()
				{
					updateControls();
				});

				connect(ui.pushButton_ChannelList, &QPushButton::clicked, [this]()
				{
					hide();
					iChannelListDialog = new ChannelListDialog(iGui, iConnection, parentWidget());
					reject();
				});

				updateControls();
			}
			~JoinChannelDialog()
			{
				iConnection.remove_observer(*this);
			}

		public:
			std::string channelName() const
			{
				return ui.lineEdit_Channel->text().toStdString();
			}
			std::string key() const
			{
				return ui.lineEdit_Key->text().toStdString();
			}
			ChannelListDialog* channelListDialog() const
			{
				return iChannelListDialog;
			}

		private:
			void updateControls()
			{
				std::vector<std::string> channels;
				neolib::tokens(ui.lineEdit_Channel->text().toStdString(), std::string(","), channels);
				bool ok = !channels.empty();
				for (auto i = channels.begin(); ok && i != channels.end(); ++i)
					ok = ok && iConnection.is_channel(*i);
				ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
			}

		private:
			virtual void connection_connecting(irc::connection& aConnection) {}
			virtual void connection_registered(irc::connection& aConnection) {}
			virtual void buffer_added(irc::buffer& aBuffer) {}
			virtual void buffer_removed(irc::buffer& aBuffer) {}
			virtual void incoming_message(irc::connection& aConnection, const irc::message& aMessage) {}
			virtual void outgoing_message(irc::connection& aConnection, const irc::message& aMessage) {}
			virtual void connection_quitting(irc::connection& aConnection) {}
			virtual void connection_disconnected(irc::connection& aConnection) { reject(); }
			virtual void connection_giveup(irc::connection& aConnection) {}

		private:
			caw::i_gui& iGui;
			irc::connection& iConnection;
			Ui::Dialog_JoinChannel ui;
			ChannelListDialog* iChannelListDialog;
		};
	}
}

#endif // JoinChannelDialog_HPP
