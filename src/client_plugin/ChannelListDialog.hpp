#ifndef ChannelListDialog_HPP
#define ChannelListDialog_HPP

#include <neolib/neolib.hpp>
#include <regex>
#include <neolib/string_utils.hpp>
#include <neolib/destroyable.hpp>
#include <QtWidgets/QDialog>
#include <QApplication>
#include <QAbstractListModel>
#include <QTimer>
#include <caw/i_gui.hpp>
#include "model.hpp"
#include "connection_manager.hpp"
#include "connection.hpp"
#include "channel_list.hpp"
#include "ui_channellist.h"

namespace caw_irc_plugin
{
	namespace gui
	{
		class ChannelListDialog : public QDialog, private irc::connection_observer, private irc::channel_list_observer, private neolib::destroyable
		{
			Q_OBJECT
		
		private:
			class ItemModel : public QAbstractListModel, private irc::channel_list_observer, private neolib::destroyable
			{
			public:
				enum EColumn
				{
					COLUMN_CHANNEL,
					COLUMN_USERS,
					COLUMN_TOPIC,
					COLUMN_BLANK,
					COLUMN_COUNT
				};
				enum ESearchType
				{
					SEARCH_TYPE_KEYWORDS,
					SEARCH_TYPE_GLOB,
					SEARCH_TYPE_REGEX
				};
			private:
				enum ESortDirection
				{
					SORT_DIRECTION_ASCENDING,
					SORT_DIRECTION_DESCENDING,
				};
				struct comparitor
				{
					ItemModel& iOwner;
					comparitor(ItemModel& aOwner) : iOwner(aOwner) {}
					comparitor(const comparitor& aOther) : iOwner(aOther.iOwner) {}
					bool operator()(const irc::channel_list_entry* aLeft, const irc::channel_list_entry* aRight) const
					{
						const irc::channel_list_entry& first = *aLeft;
						const irc::channel_list_entry& second = *aRight;
						switch (iOwner.iSortColumn)
						{
						default:
						case COLUMN_CHANNEL:
							if (irc::make_string(iOwner.iConnection, first.iChannelName) < second.iChannelName &&
								iOwner.iSortDirection == SORT_DIRECTION_ASCENDING)
								return true;
							else if (irc::make_string(iOwner.iConnection, first.iChannelName) > second.iChannelName &&
								iOwner.iSortDirection == SORT_DIRECTION_DESCENDING)
								return true;
							else
								return false;
						case COLUMN_USERS:
							if (first.iUserCount > second.iUserCount &&
								iOwner.iSortDirection == SORT_DIRECTION_ASCENDING)
								return true;
							else if (first.iUserCount < second.iUserCount &&
								iOwner.iSortDirection == SORT_DIRECTION_DESCENDING)
								return true;
							else
								return false;
						case COLUMN_TOPIC:
							{
								std::string firstChannelTopic = first.iChannelTopic;
								std::string secondChannelTopic = second.iChannelTopic;
								//formatting::parse(settings::Strip, firstChannelTopic, temp);
								//formatting::parse(settings::Strip, secondChannelTopic, temp);
								if (irc::make_string(iOwner.iConnection, firstChannelTopic) < secondChannelTopic &&
									iOwner.iSortDirection == SORT_DIRECTION_ASCENDING)
									return true;
								else if (irc::make_string(iOwner.iConnection, firstChannelTopic) > secondChannelTopic &&
									iOwner.iSortDirection == SORT_DIRECTION_DESCENDING)
									return true;
								else
									return false;
							}
						}
					}
				} iComparitor;
				typedef std::multiset<const irc::channel_list_entry*, comparitor> entries;
				typedef std::vector<const irc::channel_list_entry*> entry_index;
			public:
				ItemModel(ChannelListDialog& aDialog, QTableView& aView, irc::connection& aConnection) :
					iDialog(aDialog), iView(aView),
					iConnection(aConnection), iIcon(":/irc/Resources/channel.png"),
					iSortColumn(COLUMN_CHANNEL), iSortDirection(SORT_DIRECTION_ASCENDING),
					iComparitor(*this), iEntries(iComparitor),
					iIndexUpdating(false),
					iLastTime(neolib::thread::elapsed_ms()),
					iSearchType(SEARCH_TYPE_KEYWORDS)
				{
					iConnection.channel_list().add_observer(*this);
				}
				~ItemModel()
				{
					iConnection.channel_list().remove_observer(*this);
				}
			public:
				bool busy() const
				{
					return iIndexUpdating || iConnection.channel_list().getting_list();
				}
				void refresh()
				{
					beginResetModel();
					endResetModel();
					iView.resizeColumnsToContents();
				}
				void updateIndex(bool aReset = false)
				{
					if (iIndexUpdating)
						return;
					destroyed_flag destroyed(*this);
					iIndexUpdating = true;
					iEntryIndex.clear();
					iDialog.updateControls();
					if (aReset)
					{
						iEntries.clear();
						for (auto i = iConnection.channel_list().entries().begin(); i != iConnection.channel_list().entries().end(); ++i)
						{
							channel_list_new_entry(iConnection.channel_list(), *i);
							if (destroyed)
								return;
						}
					}
					std::vector<std::string> searchTerms;
					switch (iSearchType)
					{
					case SEARCH_TYPE_KEYWORDS:
					case SEARCH_TYPE_GLOB:
						neolib::tokens(iSearchTerm, std::string(" "), searchTerms);
						break;
					case SEARCH_TYPE_REGEX:
					default:
						if (!iSearchTerm.empty())
							searchTerms.push_back(iSearchTerm);
						break;
					}
					uint64_t lastTime = neolib::thread::elapsed_ms();
					for (auto i = iEntries.begin(); i != iEntries.end(); ++i)
					{
						bool matches = true;
						if (!searchTerms.empty())
						{
							switch (iSearchType)
							{
							case SEARCH_TYPE_KEYWORDS:
								for (auto j = searchTerms.begin(); j != searchTerms.end(); ++j)
									if (irc::make_string(iConnection, (**i).iChannelName).find(irc::make_string(iConnection, *j)) == std::string::npos && 
										irc::make_string(iConnection, (**i).iChannelTopic).find(irc::make_string(iConnection, *j)) == std::string::npos)
										matches = false;
								break;
							case SEARCH_TYPE_GLOB:
								for (auto j = searchTerms.begin(); j != searchTerms.end(); ++j)
									if (!neolib::wildcard_match(irc::make_string(iConnection, (**i).iChannelName), irc::make_string(iConnection, *j)) &&
										!neolib::wildcard_match(irc::make_string(iConnection, (**i).iChannelTopic), irc::make_string(iConnection, *j)))
										matches = false;
								break;
							case SEARCH_TYPE_REGEX:
								{
									std::regex searchTerm(searchTerms[0]);
									if (!std::regex_match(irc::make_string(iConnection, (**i).iChannelTopic), searchTerm) &&
										!std::regex_match(irc::make_string(iConnection, (**i).iChannelName), searchTerm))
										matches = false;
								}
								break;
							default:
								break;
							}

						}
						if (matches)
							iEntryIndex.push_back(*i);
						if (neolib::thread::elapsed_ms() - lastTime > 40)
						{
							lastTime = neolib::thread::elapsed_ms();
							iDialog.updateControls();
							iDialog.iGui.process_events();
							QApplication::processEvents();
							if (destroyed)
								return;
						}
					}
					refresh();
					iIndexUpdating = false;
					iDialog.updateControls();
				}
				const irc::channel_list_entry* currentEntry() const
				{
					const irc::channel_list_entry* entry = 0;
					if (iView.selectionModel()->selectedIndexes().count() > 0)
						entry = *std::next(iEntryIndex.begin(), iView.selectionModel()->selectedIndexes().at(0).row());
					return entry;
				}

			public:
				void sortBy(EColumn aColumn)
				{
					if (iSortColumn != aColumn)
					{
						iSortColumn = aColumn;
						iSortDirection = SORT_DIRECTION_ASCENDING;
					}
					else
					{
						iSortDirection = (iSortDirection == SORT_DIRECTION_ASCENDING ? SORT_DIRECTION_DESCENDING : SORT_DIRECTION_ASCENDING);
					}
					updateIndex(true);
				}
				std::pair<std::size_t, std::size_t> itemCount() const
				{
					return std::make_pair(iEntryIndex.size(), iEntries.size());
				}
				void search(const std::string& aSearchTerm, ESearchType aSearchType)
				{
					iSearchTerm = aSearchTerm;
					iSearchType = aSearchType;
					updateIndex();
				}
				const std::string& searchTerm() const
				{
					return iSearchTerm;
				}
			private:
				virtual int rowCount(const QModelIndex & /*parent*/) const
				{
					return iEntryIndex.size();
				}
				virtual int columnCount(const QModelIndex & /*parent*/) const
				{
					return COLUMN_COUNT;
				}
				virtual QVariant data(const QModelIndex &index, int role) const
				{
					const irc::channel_list_entry& entry = **(iEntryIndex.begin() + index.row());
					switch (role)
					{
					case Qt::DisplayRole:
						switch (index.column())
						{
						case COLUMN_CHANNEL:
							return QString::fromUtf8(entry.iChannelName.c_str());
						case COLUMN_USERS:
							return QString::number(entry.iUserCount);
						case COLUMN_TOPIC:
							return QString::fromUtf8(entry.iChannelTopic.c_str());
						default:
							break;
						}
						break;
					default:
						break;
					}
					return QVariant();
				}
				virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const
				{
					switch (role)
					{
					case Qt::DisplayRole:
						if (orientation == Qt::Horizontal)
						{
							switch (section)
							{
							case COLUMN_CHANNEL:
								return "Channel";
							case COLUMN_USERS:
								return "Users";
							case COLUMN_TOPIC:
								return "Topic";
							default:
								break;
							}
						}
						break;
					default:
						break;
					}
					return QVariant();
				}
			private:
				virtual void channel_list_start(const irc::channel_list& aList)
				{
					updateIndex(true);
				}
				virtual void channel_list_new_entry(const irc::channel_list& aList, const irc::channel_list_entry& aEntry)
				{
					iEntries.insert(&aEntry);
					if (neolib::thread::elapsed_ms() - iLastTime > 40)
					{
						iLastTime = neolib::thread::elapsed_ms();
						iDialog.updateControls();
						iDialog.iGui.process_events();
						QApplication::processEvents();
					}
				}
				virtual void channel_list_end(const irc::channel_list& aList)
				{
					updateIndex();
				}

			private:
				ChannelListDialog& iDialog;
				QTableView& iView;
				irc::connection& iConnection;
				QIcon iIcon;
				entries iEntries;
				entry_index iEntryIndex;
				EColumn iSortColumn;
				ESortDirection iSortDirection;
				bool iIndexUpdating;
				uint64_t iLastTime;
				ESearchType iSearchType;
				std::string iSearchTerm;
			};

		public:
			ChannelListDialog(caw::i_gui& aGui, irc::connection& aConnection, QWidget *parent = 0) :
				QDialog(parent == 0 ? QApplication::activeWindow() : parent), iGui(aGui), iConnection(aConnection)
			{
				iConnection.add_observer(*this);
				iConnection.channel_list().add_observer(*this);
				
				ui.setupUi(this);
					
				ui.tableView_Channels->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
				ui.tableView_Channels->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
				ui.tableView_Channels->horizontalHeader()->setResizeContentsPrecision(-1);

				ui.radioButton_SearchKeywords->setChecked(true);

				connect(ui.tableView_Channels->horizontalHeader(), &QHeaderView::sectionClicked, [this](int aColumn)
				{
					iItemModel->sortBy(static_cast<ItemModel::EColumn>(aColumn));
				});

				connect(ui.tableView_Channels->selectionModel(), &QItemSelectionModel::selectionChanged, [this]()
				{ 
					updateControls();
				});

				connect(ui.tableView_Channels, &QTableView::doubleClicked, [this]()
				{
					join();
				});

				connect(ui.pushButton_Refresh, &QPushButton::clicked, [this]()
				{
					refresh();
				});

				connect(ui.pushButton_Join, &QPushButton::clicked, [this]()
				{
					join();
				});

				connect(ui.pushButton_Find, &QPushButton::clicked, [this]()
				{
					iItemModel->search(ui.lineEdit_Search->text().toStdString(), 
						ui.radioButton_SearchKeywords->isChecked() ? ItemModel::SEARCH_TYPE_KEYWORDS : ui.radioButton_SearchGlob->isChecked() ? ItemModel::SEARCH_TYPE_GLOB : ItemModel::SEARCH_TYPE_REGEX);
				});

				show();
				activateWindow();

				iItemModel.reset(new ItemModel(*this, *ui.tableView_Channels, aConnection));
				ui.tableView_Channels->setModel(iItemModel.get());

				updateControls();
			}
			~ChannelListDialog()
			{
				iConnection.channel_list().remove_observer(*this);
				iConnection.remove_observer(*this);
			}

		public:
			void refresh()
			{
				if (iConnection.channel_list().no_list())
					iConnection.channel_list().list();
				iItemModel->updateIndex(true);
			}

		public:
			virtual void reject()
			{
				QDialog::reject();
				delete this;
			}
			virtual void accept()
			{
				QDialog::accept();
				delete this;
			}


		private:
			void join()
			{
				const irc::channel_list_entry* currentEntry = iItemModel->currentEntry();
				if (currentEntry != 0)
				{
					irc::message theMessage(iConnection, irc::message::OUTGOING);
					theMessage.set_command(irc::message::JOIN);
					theMessage.parameters().push_back(currentEntry->iChannelName);
					iConnection.send_message(theMessage);
					QTimer::singleShot(0, this, SLOT(accept()));
				}
			}
			void updateControls()
			{
				ui.pushButton_Join->setEnabled(iItemModel->currentEntry() != 0);
				ui.pushButton_Refresh->setDisabled(iItemModel->busy());
				ui.pushButton_Find->setDisabled(iItemModel->busy());
				if (iItemModel->busy())
					setCursor(Qt::WaitCursor);
				else
					unsetCursor();
				updateSummary();
			}
			void updateSummary()
			{
				std::pair<std::size_t, std::size_t> counts = iItemModel->itemCount();
				if (counts.first == counts.second)
					ui.label_Summary->setText(
						QString("%1 channel(s)").arg(counts.first) + 
						(!iItemModel->searchTerm().empty() ? QString(", search: '%1'").arg(iItemModel->searchTerm().c_str()) : QString()));
				else
					ui.label_Summary->setText(
						QString("%1 of %2 channel(s)").arg(counts.first).arg(counts.second) + 
						(!iItemModel->searchTerm().empty() ? QString(", search: '%1'").arg(iItemModel->searchTerm().c_str()) : QString()));
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
			virtual void channel_list_start(const irc::channel_list& aList)
			{
				updateSummary();
				updateControls();
			}
			virtual void channel_list_new_entry(const irc::channel_list& aList, const irc::channel_list_entry& aEntry)
			{
				updateSummary();
				updateControls();
			}
			virtual void channel_list_end(const irc::channel_list& aList)
			{
				updateSummary();
				updateControls();
			}

		private:
			caw::i_gui& iGui;
			irc::connection& iConnection;
			Ui::Dialog_ChannelList ui;
			std::unique_ptr<ItemModel> iItemModel;
		};
	}
}

#endif // ChannelListDialog_HPP
