#ifndef ConnectionPasswordDialog_HPP
#define ConnectionPasswordDialog_HPP

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
#include <neolib/i_settings.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_hook.hpp>
#include "ui_connectionpassword.h"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ConnectionPasswordDialog : public QDialog
		{
			Q_OBJECT

		private:

		public:
			ConnectionPasswordDialog(neolib::io_thread& aThread, caw::i_gui& aGui, neolib::i_settings& aSettings, const std::string& aServer, const std::string& aIdentity, QWidget *parent = 0) :
				QDialog(parent),
				iGui(aGui),
				iSettings(aSettings),
				iRememberPassword(false),
				iTimer(caw::qt::hook::instance().gui_thread(), std::bind(&ConnectionPasswordDialog::animate, this, std::placeholders::_1), 40),
				iLastChangeTime(neolib::thread::program_elapsed_ms()),
				iShowHashSetting(aSettings.find_setting("Connection", "PasswordEntryGlyphs").value().value_as_boolean()),
				iShowHash(false)
			{
				iGui.activate_main_window();
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
				ui.setupUi(this);
				ui.lineEditServer->setText(aServer.c_str());
				ui.lineEditIdentity->setText(aIdentity.c_str());
				ui.checkBox_ShowHashSetting->setChecked(iShowHashSetting);
				connect(ui.lineEditConnectionPassword, &QLineEdit::textChanged, this, &ConnectionPasswordDialog::passwordChanged);
				connect(ui.checkBox_ShowHashSetting, &QCheckBox::stateChanged, this, &ConnectionPasswordDialog::showHashChanged);
			}

		public:
			const std::string& getPassword() const
			{
				return iPassword;
			}
			bool rememberPassword() const
			{
				return iRememberPassword;
			}

			private slots :
			virtual void done(int result)
			{
				if (result == QDialog::Accepted)
				{
					iPassword = ui.lineEditConnectionPassword->text().toUtf8();
					iRememberPassword = ui.checkBoxRememberPassword->isChecked();
				}
				QDialog::done(result);
			}
			void passwordChanged()
			{
				iPreviousPassword = iPassword;
				iPassword = ui.lineEditConnectionPassword->text().toUtf8();
				iLastChangeTime = neolib::thread::program_elapsed_ms();
			}
			void showHashChanged()
			{
				if (!iShowHashSetting && ui.checkBox_ShowHashSetting->isChecked() &&
					QMessageBox::question(this, "Password Verification Glyph", ui.checkBox_ShowHashSetting->whatsThis(), QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
				{
					ui.checkBox_ShowHashSetting->setCheckState(Qt::CheckState::Unchecked);
					return;
				}
				iShowHashSetting = ui.checkBox_ShowHashSetting->isChecked();
				iSettings.change_setting(iSettings.find_setting("Connection", "PasswordEntryGlyphs"), iShowHashSetting, true);
			}
		private:
			virtual void paintEvent(QPaintEvent* pe)
			{
				QDialog::paintEvent(pe);

				if (!iShowHashSetting || !iShowHash)
					return;

				boost::uuids::detail::sha1 sha1;
				sha1.process_bytes(iPassword.c_str(), iPassword.size());
				uint32_t hash[5];
				sha1.get_digest(hash);
				for (std::size_t i = 1; i < 5; ++i)
					hash[0] ^= hash[i];

				uint64_t timeSinceChange = neolib::thread::program_elapsed_ms() - iLastChangeTime;
				neolib::random r(iPassword.size() > 4 && timeSinceChange > 500ull ? hash[0] : std::random_device()());

				QPainter painter;
				painter.begin(this);
				painter.setRenderHint(QPainter::Antialiasing);

				for (uint32_t i = 0; i < 5; ++i)
				{
					QFont font;
					font.setPointSize(i == 0 ? 256 : 64);
					painter.setFont(font);
					QPen pen(hashColours()[r.get(hashColours().size() - 1)]);
					painter.setPen(pen);
					painter.setOpacity(0.25f);
					if (i == 0)
						painter.drawText(QRect(0, 0, QDialog::width(), QDialog::height()), Qt::AlignVCenter | Qt::AlignHCenter, QChar(r.get(5) + 0x265A));
					else
						painter.drawText(QRect(QDialog::width() * (i - 1) / 4, 0, QDialog::width() / 4, QDialog::height()), Qt::AlignBottom | Qt::AlignHCenter, QChar(r.get(5) + 0x265A));
				}

				painter.end();
			}
			void animate(neolib::timer_callback& aTimer)
			{
				aTimer.again();
				uint64_t timeSinceChange = neolib::thread::program_elapsed_ms() - iLastChangeTime;
				if (!iPassword.empty() && (iPassword.size() > 4 || (iPassword.size() > iPreviousPassword.size() && timeSinceChange < 500ull)))
					iShowHash = true;
				else
					iShowHash = false;
				update();
			}
			const std::array<QColor, 6>& hashColours() const
			{
				static const std::array<QColor, 6> sDarkHashColours =
				{
					QColor(192, 0, 0),
					QColor(0, 192, 0),
					QColor(0, 0, 192),
					QColor(192, 192, 0),
					QColor(0, 192, 192),
					QColor(192, 0, 192)
				};
				static const std::array<QColor, 6> sLightHashColours =
				{
					QColor(255, 128, 128),
					QColor(128, 255, 128),
					QColor(128, 128, 255),
					QColor(255, 255, 128),
					QColor(128, 255, 255),
					QColor(255, 128, 255)
				};
				return iGui.dark_controls() ? sLightHashColours : sDarkHashColours;
			}

		private:
			caw::i_gui& iGui;
			Ui::connectionPassword ui;
			neolib::i_settings& iSettings;
			std::string iPassword;
			bool iRememberPassword;
			neolib::timer_callback iTimer;
			std::string iPreviousPassword;
			uint64_t iLastChangeTime;
			bool iShowHashSetting;
			bool iShowHash;
		};
	}
}

#endif // SettingsDialog_HPP
