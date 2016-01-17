#ifndef ManageMacrosDialog_HPP
#define ManageMacrosDialog_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <boost/uuid/sha1.hpp>
#include <boost/filesystem.hpp>
#include <QtWidgets/QDialog>
#include <QPainter>
#include <QMessageBox>
#include <QAbstractListModel>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <neolib/i_application.hpp>
#include <caw/i_gui.hpp>
#include <irc/client/model.hpp>
#include <irc/client/macros.hpp>
#include <caw/qt/qt_utils.hpp>
#include <caw/qt/qt_hook.hpp>

#include "ui_managemacros.h"

#include "MacroDialog.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ManageMacrosDialog : public QDialog, private irc::macros_observer
		{
			Q_OBJECT

		private:
			class ItemModel : public QAbstractListModel
			{
			public:
				ItemModel(irc::model& aModel) :
					iModel(aModel), iIcon(":/irc/Resources/macro.png")
				{
				}
				~ItemModel()
				{
				}
			public:
				void refresh()
				{
					beginResetModel();
					endResetModel();
				}
			private:
				virtual int rowCount(const QModelIndex & /*parent*/) const
				{
					return iModel.macros().entries().size();
				}
				virtual QVariant data(const QModelIndex &index, int role) const
				{
					irc::macro& macro = *std::next(iModel.macros().entries().begin(), index.row());
					switch (role)
					{
					case Qt::DisplayRole:
						switch (index.column())
						{
						case 0:
							return QString::fromUtf8(macro.name().c_str());
						default:
							break;
						}
						break;
					case Qt::DecorationRole:
						switch (index.column())
						{
						case 0:
							return iIcon;
						default:
							break;
						}
						break;
					default:
						break;
					}
					return QVariant();
				}
			private:
				irc::model& iModel;
				QIcon iIcon;
			};

		public:
			ManageMacrosDialog(neolib::i_application& aApplication, irc::model& aModel, QWidget *parent = 0) :
				QDialog(parent), iApplication(aApplication), iModel(aModel), iItemModel(aModel)
			{
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				ui.setupUi(this);
				ui.listView_Macros->setModel(&iItemModel);
				iModel.macros().add_observer(*this);
				for (irc::macros::container_type::const_iterator i = iModel.macros().entries().begin(); i != iModel.macros().entries().end(); ++i)
				{
					macro_added(*i);
				}
				connect(ui.listView_Macros->selectionModel(), &QItemSelectionModel::selectionChanged, [this](){ updateControls(); });
				connect(ui.listView_Macros, &QListView::doubleClicked, [this]() { editItem(); });
				connect(ui.pushButton_New, &QPushButton::clicked, [this]()
				{
					MacroDialog dialog(iModel, irc::macro(), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::macro newMacro;
						dialog.getMacro(newMacro);
						iModel.macros().add(newMacro);
					}
				});
				connect(ui.pushButton_Edit, &QPushButton::clicked, [this]() { editItem(); });
				connect(ui.pushButton_Remove, &QPushButton::clicked, [this]()
				{
					irc::macro* macro = 0;
					if (ui.listView_Macros->selectionModel()->selectedIndexes().count() > 0)
						macro = &*std::next(iModel.macros().entries().begin(), ui.listView_Macros->selectionModel()->selectedIndexes().at(0).row());
					if (macro != 0)
						iModel.macros().remove(*macro);
				});
				connect(ui.pushButton_Reset, &QPushButton::clicked, [this]()
				{
					if (QMessageBox::question(this, "Manage Macros", "Reset macro list to default?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
						return;

					try {
						boost::filesystem::copy_file(boost::filesystem::path(iApplication.info().application_folder().to_std_string() + "/macros.xml"),
							boost::filesystem::path(iModel.root_path() + "macros.xml"), boost::filesystem::copy_option::overwrite_if_exists);
					}
					catch (...) {}
					iModel.macros().clear();
					read_macros(iModel, []()->bool
					{
						return QMessageBox::warning(NULL, "Configuration File Error", "The macros file is corrupt. Delete it?", QMessageBox::No | QMessageBox::Yes) == QMessageBox::Yes;
					});
					iItemModel.refresh();
				});
				updateControls();
			}
			~ManageMacrosDialog()
			{
				iModel.macros().remove_observer(*this);
			}

		public:

			private slots :
				virtual void done(int result)
			{
				QDialog::done(result);
			}
		private:
			virtual void macro_added(const irc::macro& aEntry)
			{
				iItemModel.refresh();
				updateControls();
			}
			virtual void macro_updated(const irc::macro& aEntry)
			{
				iItemModel.refresh();
				updateControls();
			}
			virtual void macro_removed(const irc::macro& aEntry)
			{
				iItemModel.refresh();
				updateControls();
			}
			virtual void macro_syntax_error(const irc::macro& aEntry, std::size_t aLineNumber, irc::macro::error aError)
			{
			}
		private:
			void editItem()
			{
				irc::macro* macro = 0;
				if (ui.listView_Macros->selectionModel()->selectedIndexes().count() > 0)
					macro = &*std::next(iModel.macros().entries().begin(), ui.listView_Macros->selectionModel()->selectedIndexes().at(0).row());
				if (macro != 0)
				{
					MacroDialog dialog(iModel, *macro, this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::macro editedMacro;
						dialog.getMacro(editedMacro);
						iModel.macros().update(*macro, editedMacro);
					}
				}
			}
			void updateControls()
			{
				irc::macro* macro = 0;
				if (ui.listView_Macros->selectionModel()->selectedIndexes().count() > 0)
					macro = &*std::next(iModel.macros().entries().begin(), ui.listView_Macros->selectionModel()->selectedIndexes().at(0).row());
				ui.pushButton_New->setEnabled(true);
				ui.pushButton_Edit->setEnabled(macro != 0);
				ui.pushButton_Remove->setEnabled(macro != 0);
			}

		private:
			neolib::i_application& iApplication;
			irc::model& iModel;
			Ui::dialog_ManageMacros ui;
			ItemModel iItemModel;
		};
	}
}

#endif // ManageMacrosDialog_HPP
