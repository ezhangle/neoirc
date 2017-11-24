#ifndef ManageIdentitiesDialog_HPP
#define ManageIdentitiesDialog_HPP

#include <neolib/neolib.hpp>
#include <functional>
#include <array>
#include <random>
#include <boost/uuid/sha1.hpp>
#include <QtWidgets/QDialog>
#include <QPainter>
#include <QMessageBox>
#include <QAbstractListModel>
#include <neolib/random.hpp>
#include <neolib/timer.hpp>
#include <caw/i_gui.hpp>
#include <caw/qt/qt_utils.hpp>
#include <caw/qt/qt_hook.hpp>

#include "ui_manageidentities.h"

#include "model.hpp"
#include "identity.hpp"

#include "IdentityDialog.hpp"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ManageIdentitiesDialog : public QDialog, private irc::identities_observer
		{
			Q_OBJECT

		private:
			class ItemModel : public QAbstractListModel
			{
			public:
				ItemModel(irc::model& aModel) :
					iModel(aModel), iIcon(":/irc/Resources/user.png")
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
					return iModel.identities().identity_list().size();
				}
				virtual QVariant data(const QModelIndex &index, int role) const
				{
					irc::identity& identity = *std::next(iModel.identities().identity_list().begin(), index.row());
					switch (role)
					{
					case Qt::DisplayRole:
						switch (index.column())
						{
						case 0:
							return QString::fromUtf8(identity.nick_name().c_str());
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
			ManageIdentitiesDialog(irc::model& aModel, QWidget *parent = 0) :
				QDialog(parent), iModel(aModel), iItemModel(aModel)
			{
				setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
				ui.setupUi(this);
				ui.listView_Identities->setModel(&iItemModel);
				iModel.identities().add_observer(*this);
				for (irc::identities::container_type::const_iterator i = iModel.identities().identity_list().begin(); i != iModel.identities().identity_list().end(); ++i)
				{
					identity_added(*i);
				}
				connect(ui.listView_Identities->selectionModel(), &QItemSelectionModel::selectionChanged, [this](){ updateControls(); });
				connect(ui.listView_Identities, &QListView::doubleClicked, [this]() { editItem(); });
				connect(ui.pushButton_New, &QPushButton::clicked, [this]()
				{
					IdentityDialog dialog(iModel, irc::identity(), this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::identity id;
						bool isDefault;
						dialog.getIdentity(id, isDefault);
						if (isDefault)
							iModel.set_default_identity(id);
						iModel.identities().add_item(id);
					}
				});
				connect(ui.pushButton_Edit, &QPushButton::clicked, [this]() { editItem(); });
				connect(ui.pushButton_Remove, &QPushButton::clicked, [this]()
				{
					irc::identity* identity = 0;
					if (ui.listView_Identities->selectionModel()->selectedIndexes().count() > 0)
						identity = &*std::next(iModel.identities().identity_list().begin(), ui.listView_Identities->selectionModel()->selectedIndexes().at(0).row());
					if (identity != 0)
						iModel.identities().delete_item(*identity);
				});
				connect(ui.pushButton_ClearPasswords, &QPushButton::clicked, [this]()
				{
					irc::identity* identity = 0;
					if (ui.listView_Identities->selectionModel()->selectedIndexes().count() > 0)
						identity = &*std::next(iModel.identities().identity_list().begin(), ui.listView_Identities->selectionModel()->selectedIndexes().at(0).row());
					if (identity != 0)
					{
						identity->passwords().clear();
						iModel.identities().update_item(*identity, *identity);
					}
				});
				connect(ui.comboBox_DefaultIdentity, (void(QComboBox::*)(int))&QComboBox::currentIndexChanged, [this]()
				{
					int index = ui.comboBox_DefaultIdentity->currentIndex();
					if (index != -1)
					{
						iModel.set_default_identity(caw::qt::QVariant_to_user_type<const irc::identity&>(ui.comboBox_DefaultIdentity->itemData(index)));
						irc::write_identity_list(iModel, iModel.default_identity().nick_name());
					}
				});
				updateControls();
			}
			~ManageIdentitiesDialog()
			{
				iModel.identities().remove_observer(*this);
			}

		public:

			private slots :
				virtual void done(int result)
			{
				QDialog::done(result);
			}
		private:
			virtual void identity_added(const irc::identity& aEntry)
			{
				iItemModel.refresh();
				ui.comboBox_DefaultIdentity->addItem(QIcon(":/irc/Resources/user.png"), aEntry.nick_name().c_str(), caw::qt::user_type_to_QVariant(aEntry));
				updateControls();
			}
			virtual void identity_updated(const irc::identity& aEntry, const irc::identity& aOldEntry)
			{
				iItemModel.refresh();
				int index = ui.comboBox_DefaultIdentity->findData(caw::qt::user_type_to_QVariant(aEntry));
				ui.comboBox_DefaultIdentity->setItemText(index, aEntry.nick_name().c_str());
				updateControls();
			}
			virtual void identity_removed(const irc::identity& aEntry)
			{
				iItemModel.refresh();
				int index = ui.comboBox_DefaultIdentity->findData(caw::qt::user_type_to_QVariant(aEntry));
				ui.comboBox_DefaultIdentity->removeItem(index);
				updateControls();
			}
		private:
			void editItem()
			{
				irc::identity* identity = 0;
				if (ui.listView_Identities->selectionModel()->selectedIndexes().count() > 0)
					identity = &*std::next(iModel.identities().identity_list().begin(), ui.listView_Identities->selectionModel()->selectedIndexes().at(0).row());
				if (identity != 0)
				{
					IdentityDialog dialog(iModel, *identity, this);
					if (dialog.exec() == QDialog::Accepted)
					{
						irc::identity id;
						bool isDefault;
						dialog.getIdentity(id, isDefault);
						if (isDefault)
							iModel.set_default_identity(id);
						iModel.identities().update_item(*identity, id);
					}
				}
			}
			void updateControls()
			{
				int selectionIndex = -1;
				for (int i = 0; selectionIndex == -1 && i < ui.comboBox_DefaultIdentity->count(); ++i)
				{
					if (caw::qt::QVariant_to_user_type<const irc::identity&>(ui.comboBox_DefaultIdentity->itemData(i)).nick_name() == iModel.default_identity().nick_name())
						selectionIndex = i;
				}
				ui.comboBox_DefaultIdentity->setCurrentIndex(selectionIndex);
				irc::identity* identity = 0;
				if (ui.listView_Identities->selectionModel()->selectedIndexes().count() > 0)
					identity = &*std::next(iModel.identities().identity_list().begin(), ui.listView_Identities->selectionModel()->selectedIndexes().at(0).row());
				ui.pushButton_New->setEnabled(true);
				ui.pushButton_Edit->setEnabled(identity != 0);
				ui.pushButton_Remove->setEnabled(identity != 0);
				ui.pushButton_ClearPasswords->setEnabled(identity != 0 && !identity->passwords().empty());
			}

		private:
			irc::model& iModel;
			Ui::dialog_ManageIdentities ui;
			ItemModel iItemModel;
		};
	}
}

#endif // ManageIdentitiesDialog_HPP
