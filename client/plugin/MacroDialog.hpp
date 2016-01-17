#ifndef MacroDialog_HPP
#define MacroDialog_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <boost/uuid/sha1.hpp>
#include <QtWidgets/QDialog>
#include <QPainter>
#include <QMessageBox>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <caw/i_gui.hpp>
#include <irc/client/model.hpp>
#include <irc/client/message.hpp>
#include <irc/client/buffer.hpp>
#include <irc/client/macros.hpp>
#include <caw/qt/qt_hook.hpp>
#include "ui_macro.h"

namespace caw_irc_plugin
{
	namespace gui
	{
		class MacroDialog : public QDialog
		{
			Q_OBJECT

		private:

		public:
			MacroDialog(irc::model& aModel, const irc::macro& aMacro = irc::macro(), QWidget *parent = 0) :
				QDialog(parent), iModel(aModel), iMacro(aMacro)
			{
				ui.setupUi(this);
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				setWindowTitle(aMacro == irc::macro() ? "New Macro" : "Edit Macro");
				ui.lineEdit_Macro->setText(aMacro.name().c_str());
				ui.lineEdit_Description->setText(aMacro.description().c_str());
				ui.checkBox_Enabled->setChecked(aMacro.enabled());
				ui.checkBox_AddToUserMenu->setChecked(aMacro.user_menu());
				ui.plainTextEdit_Commands->setPlainText(aMacro.script().c_str());
			}

		public:
			void getMacro(irc::macro& aResult)
			{
				aResult = iMacro;
				aResult.set_name(ui.lineEdit_Macro->text().toStdString());
				aResult.set_description(ui.lineEdit_Description->text().toStdString());
				aResult.set_enabled(ui.checkBox_Enabled->isChecked());
				aResult.set_user_menu(ui.checkBox_AddToUserMenu->isChecked());
				aResult.set_script(ui.plainTextEdit_Commands->toPlainText().toStdString());
			}

		private slots :
			virtual void done(int result)
			{
				if (result == QDialog::Accepted)
				{
					irc::macro editedMacro;
					getMacro(editedMacro);
					if (editedMacro.name().empty() || editedMacro.name() == "/")
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Please enter a macro name", QMessageBox::Ok);
						return;
					}
					if (editedMacro.script().empty())
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Please enter macro command(s)", QMessageBox::Ok);
						return;
					}
					std::string command = editedMacro.name().substr(1);
					irc::message m(iModel.connection_manager(), irc::message::OUTGOING);
					m.parse_command(command);
					if (m.command() != irc::message::UNKNOWN ||
						irc::get_internal_command(command).first != irc::internal_command::UNKNOWN)
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Macro name is reserved, choose another", QMessageBox::Ok);
						return;
					}
					if (editedMacro.name() != iMacro.name() &&
						iModel.macros().has_macro(editedMacro.name()))
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", "Macro with that name already exists, choose another", QMessageBox::Ok);
						return;
					}
				}
				QDialog::done(result);
			}

		private:
			irc::model& iModel;
			irc::macro iMacro;
			Ui::dialog_Macro ui;
		};
	}
}

#endif // SettingsDialog_HPP
