#ifndef IdentityDialog_HPP
#define IdentityDialog_HPP

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
#include <caw/qt/qt_hook.hpp>
#include "ui_identity.h"

#include "model.hpp"
#include "identity.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		namespace
		{
			const std::string sValidNickNameCharacters =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-[]\\`_^{|}";
			const std::string sValidNickNameFirstCharacter =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz[]\\`_^{|}";
		}

		class IdentityDialog : public QDialog
		{
			Q_OBJECT

		private:

		public:
			IdentityDialog(irc::model& aModel, const irc::identity& aIdentity = irc::identity(), QWidget *parent = 0) :
				QDialog(parent), iIdentity(aIdentity)
			{
				ui.setupUi(this);
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				setWindowTitle(aModel.identities().find_item(aIdentity) == aModel.identities().identity_list().end() ? "New Identity" : "Edit Identity");
				ui.lineEdit_Nickname->setText(aIdentity.nick_name().c_str());
				ui.lineEdit_FullName->setText(aIdentity.full_name().c_str());
				ui.lineEdit_EmailAddress->setText(aIdentity.email_address().c_str());
				ui.checkBox_DefaultIdentity->setChecked(aModel.identities().empty() || aModel.default_identity().nick_name() == aIdentity.nick_name());
				if (aModel.identities().empty())
					ui.checkBox_DefaultIdentity->setEnabled(false);
				ui.checkBox_Invisible->setChecked(aIdentity.invisible());
				for (std::size_t i = 0; i < aIdentity.alternate_nick_names().size(); ++i)
					ui.comboBox_AlternateNicknames->addItem(aIdentity.alternate_nick_names()[i].c_str());
				updateControls();
				ui.comboBox_AlternateNicknames->lineEdit()->setMaxLength(ui.lineEdit_Nickname->maxLength());
				connect(ui.pushButton_AddAlternateNickname, &QPushButton::clicked, [this](){
					ui.comboBox_AlternateNicknames->addItem(ui.comboBox_AlternateNicknames->currentText());
					ui.comboBox_AlternateNicknames->setCurrentIndex(ui.comboBox_AlternateNicknames->count() - 1);
					ui.comboBox_AlternateNicknames->setCurrentText("");
					updateControls();
				});
				connect(ui.pushButton_RemoveAlternateNickname, &QPushButton::clicked, [this](){
					ui.comboBox_AlternateNicknames->setCurrentText(ui.comboBox_AlternateNicknames->itemText(ui.comboBox_AlternateNicknames->currentIndex()));
					ui.comboBox_AlternateNicknames->removeItem(ui.comboBox_AlternateNicknames->currentIndex());
					updateControls();
				});
				connect(ui.comboBox_AlternateNicknames, &QComboBox::currentTextChanged, [this](){
					updateControls();
				});
			}

		public:
			void getIdentity(irc::identity& aResult, bool& aDefaultIdentity)
			{
				aResult = iIdentity;
				aResult.set_nick_name(ui.lineEdit_Nickname->text().toStdString());
				aResult.set_full_name(ui.lineEdit_FullName->text().toStdString());
				aResult.set_email_address(ui.lineEdit_EmailAddress->text().toStdString());
				aResult.set_invisible(ui.checkBox_Invisible->isChecked());
				aResult.alternate_nick_names().clear();
				for (int i = 0; i < ui.comboBox_AlternateNicknames->count(); ++i)
					aResult.alternate_nick_names().push_back(ui.comboBox_AlternateNicknames->itemText(i).toStdString());
				aDefaultIdentity = ui.checkBox_DefaultIdentity->isChecked();
			}

			private slots :
			virtual void done(int result)
			{
				if (result == QDialog::Accepted)
				{
					bool invalidStartCharacter = false;
					if (ui.lineEdit_Nickname->text().isEmpty() || sValidNickNameFirstCharacter.find(ui.lineEdit_Nickname->text().toStdString()[0]) == std::string::npos)
						invalidStartCharacter = true;
					for (int i = 0; !invalidStartCharacter && i < ui.comboBox_AlternateNicknames->count(); ++i)
						if (ui.comboBox_AlternateNicknames->itemText(i).toStdString().empty() || sValidNickNameFirstCharacter.find(ui.comboBox_AlternateNicknames->itemText(i).toStdString()[0]) == std::string::npos)
							invalidStartCharacter = true;
					if (invalidStartCharacter)
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", ("Valid nicknames must begin with one of the following characters:\n\n" + sValidNickNameFirstCharacter).c_str(), QMessageBox::Ok);
						return;
					}
					bool invalidCharacters = false;
					if (ui.lineEdit_Nickname->text().toStdString().find_first_not_of(sValidNickNameCharacters, 0) != std::string::npos)
						invalidCharacters = true;
					for (int i = 0; !invalidCharacters && i < ui.comboBox_AlternateNicknames->count(); ++i)
						if (ui.comboBox_AlternateNicknames->itemText(i).toStdString().find_first_not_of(sValidNickNameCharacters, 0) != std::string::npos)
							invalidCharacters = true;
					if (invalidCharacters)
					{
						QMessageBox::critical(this, "ClicksAndWhistles - Invalid Input", ("Valid nicknames must only contain the following characters:\n\n" + sValidNickNameCharacters).c_str(), QMessageBox::Ok);
						return;
					}
				}
				QDialog::done(result);
			}
		private:
			void updateControls()
			{
				ui.pushButton_AddAlternateNickname->setEnabled(!ui.comboBox_AlternateNicknames->currentText().isEmpty() && ui.comboBox_AlternateNicknames->findText(ui.comboBox_AlternateNicknames->currentText()) == -1);
				ui.pushButton_RemoveAlternateNickname->setEnabled(!ui.comboBox_AlternateNicknames->currentText().isEmpty() && ui.comboBox_AlternateNicknames->currentIndex() != -1 &&
					ui.comboBox_AlternateNicknames->findText(ui.comboBox_AlternateNicknames->currentText()) == ui.comboBox_AlternateNicknames->currentIndex());
			}

		private:
			irc::identity iIdentity;
			Ui::identity ui;
		};
	}
}

#endif // SettingsDialog_HPP
