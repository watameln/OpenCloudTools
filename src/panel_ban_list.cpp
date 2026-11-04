#include "panel_ban_list.h"

#include <optional>
#include <vector>

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QPushButton>
#include <QVBoxLayout>

#include "assert.h"
#include "data_request.h"
#include "diag_confirm_change.h"
#include "diag_operation_in_progress.h"
#include "model_common.h"
#include "model_qt.h"
#include "profile.h"
#include "util_enum.h"
#include "window_ban_view.h"

BanListPanel::BanListPanel(QWidget* parent, const QString& api_key, const std::shared_ptr<UniverseProfile>& universe) :
	QWidget{ parent }, api_key{ api_key }, attached_universe{ universe }
{
	setMinimumSize(500, 300);
	QLabel* const title = new QLabel{ "Ban List", this };
	QFont title_font = title->font();
	title_font.setPointSize(15);
	title_font.setBold(true);
	title->setFont(title_font);
	QLabel* const subtitle = new QLabel{ "Search, review and manage experience restrictions", this };
	subtitle->setObjectName("toolbarHint");
	QVBoxLayout* const heading_layout = new QVBoxLayout{};
	heading_layout->setSpacing(2);
	heading_layout->addWidget(title);
	heading_layout->addWidget(subtitle);
	QWidget* const search_row = new QWidget{ this };
	search_edit = new QLineEdit{ search_row };
	search_edit->setClearButtonEnabled(true);
	search_edit->setPlaceholderText("Search user ID, status or reason...");
	search_edit->setMinimumWidth(240);
	active_only_check = new QCheckBox{ "Active bans only", search_row };
	active_only_check->setChecked(true);
	QPushButton* const refresh_button = new QPushButton{ "Refresh", search_row };
	connect(refresh_button, &QPushButton::clicked, this, &BanListPanel::pressed_refresh);
	QHBoxLayout* const search_layout = new QHBoxLayout{ search_row };
	search_layout->setContentsMargins(0, 0, 0, 0);
	search_layout->addWidget(search_edit, 1);
	search_layout->addWidget(active_only_check);
	search_layout->addWidget(refresh_button);
	filter_model = new QSortFilterProxyModel{ this };
	filter_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
	filter_model->setFilterKeyColumn(-1);
	filter_model->setDynamicSortFilter(true);
	connect(search_edit, &QLineEdit::textChanged, this, [this](const QString& text) {
		filter_model->setFilterFixedString(text);
		update_result_count();
	});
	tree_view = new QTableView{ this };
	tree_view->setModel(filter_model);
	tree_view->setSelectionBehavior(QAbstractItemView::SelectRows);
	tree_view->setSelectionMode(QAbstractItemView::SingleSelection);
	tree_view->setAlternatingRowColors(true);
	tree_view->setShowGrid(false);
	tree_view->setWordWrap(false);
	tree_view->setTextElideMode(Qt::ElideRight);
	tree_view->verticalHeader()->hide();
	tree_view->verticalHeader()->setDefaultSectionSize(30);
	tree_view->horizontalHeader()->setStretchLastSection(false);
	tree_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	tree_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	tree_view->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	tree_view->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	tree_view->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
	tree_view->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
	tree_view->setSortingEnabled(true);
	tree_view->sortByColumn(2, Qt::DescendingOrder);
	connect(tree_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &BanListPanel::handle_selected_ban_changed);
	connect(tree_view, &QTableView::doubleClicked, this, [this](const QModelIndex&) { pressed_details(); });
	QWidget* const actions = new QWidget{ this };
	count_label = new QLabel{ "0 bans loaded", actions };
	count_label->setObjectName("toolbarHint");
	unban_button = new QPushButton{ "Unban...", actions };
	connect(unban_button, &QPushButton::clicked, this, &BanListPanel::pressed_unban);
	edit_button = new QPushButton{ "Edit...", actions };
	connect(edit_button, &QPushButton::clicked, this, &BanListPanel::pressed_edit);
	details_button = new QPushButton{ "View Details", actions };
	connect(details_button, &QPushButton::clicked, this, &BanListPanel::pressed_details);
	QHBoxLayout* const actions_layout = new QHBoxLayout{ actions };
	actions_layout->setContentsMargins(0, 0, 0, 0);
	actions_layout->addWidget(count_label);
	actions_layout->addStretch();
	actions_layout->addWidget(unban_button);
	actions_layout->addWidget(edit_button);
	actions_layout->addWidget(details_button);
	QVBoxLayout* const layout = new QVBoxLayout{ this };
	layout->setContentsMargins(16, 14, 16, 14);
	layout->setSpacing(12);
	layout->addLayout(heading_layout);
	layout->addWidget(search_row);
	layout->addWidget(tree_view, 1);
	layout->addWidget(actions);
	set_table_model(nullptr);
	gui_refresh();
}

void BanListPanel::gui_refresh()
{
	const bool enabled = get_selected_single_index().isValid();
	unban_button->setEnabled(enabled);
	details_button->setEnabled(enabled);
	edit_button->setEnabled(enabled);
}

void BanListPanel::update_result_count()
{
	const QAbstractItemModel* const source = filter_model->sourceModel();
	const int loaded = source ? source->rowCount() : 0;
	count_label->setText(QString{ "%1 shown / %2 loaded" }.arg(filter_model->rowCount()).arg(loaded));
}

QModelIndex BanListPanel::get_selected_single_index() const
{
	const QItemSelectionModel* const select_model = tree_view->selectionModel();
	if (select_model == nullptr)
	{
		return QModelIndex{};
	}

	if (select_model->selectedRows().count() != 1)
	{
		return QModelIndex{};
	}

	return select_model->selectedRows().front();
}

std::optional<BanListUserRestriction> BanListPanel::get_selected_restriction() const
{
	const QModelIndex selected_index = get_selected_single_index();

	if (selected_index.isValid() == false)
	{
		return std::nullopt;
	}

	const BanListQTableModel* const table_model = dynamic_cast<const BanListQTableModel*>(filter_model->sourceModel());
	if (table_model == nullptr)
	{
		return std::nullopt;
	}

	return table_model->get_restriction(filter_model->mapToSource(selected_index).row());
}

void BanListPanel::set_table_model(BanListQTableModel* entry_model)
{
	if (entry_model == nullptr) entry_model = new BanListQTableModel{ this, {} };
	QAbstractItemModel* const old_model = filter_model->sourceModel();
	filter_model->setSourceModel(entry_model);
	if (old_model && old_model != entry_model) old_model->deleteLater();
	update_result_count();
	gui_refresh();
}

void BanListPanel::handle_selected_ban_changed()
{
	gui_refresh();
}

void BanListPanel::pressed_details()
{
	const std::optional<BanListUserRestriction> opt_restriction = get_selected_restriction();

	if (!opt_restriction) return;
	ViewBanWindow* const ban_window = new ViewBanWindow{ ViewEditMode::View, "", *opt_restriction, this };
	ban_window->show();
}

void BanListPanel::pressed_edit()
{
	const std::optional<BanListUserRestriction> opt_restriction = get_selected_restriction();

	if (!opt_restriction) return;
	ViewBanWindow* const ban_window = new ViewBanWindow{ ViewEditMode::Edit, api_key, *opt_restriction, this };
	ban_window->show();
}

void BanListPanel::pressed_refresh()
{
	auto universe = attached_universe.lock();
	if (!universe) return;

	const bool active_only = active_only_check->isChecked();
	auto req = std::make_shared<UserRestrictionGetListV2Request>(api_key, universe->get_universe_id(), active_only);
	OperationInProgressDialog diag{ this, req };
	diag.exec();
	if (!req->req_success()) return;

	BanListQTableModel* const new_model = new BanListQTableModel{ this, req->get_restrictions() };
	set_table_model(new_model);
}

void BanListPanel::pressed_unban()
{
	const std::optional<BanListUserRestriction> opt_restriction = get_selected_restriction();
	if (!opt_restriction)
	{
		return;
	}

	ConfirmChangeDialog* const confirm_dialog = new ConfirmChangeDialog{ this, ChangeType::BanListUnbanUser, opt_restriction->get_user()};
	bool confirmed = static_cast<bool>(confirm_dialog->exec());
	if (!confirmed)
	{
		return;
	}

	constexpr bool active = false;
	const std::optional<QString> duration;
	const QString reason = "Manually unbanned";
	constexpr bool exclude_alts = false;
	const BanListGameJoinRestrictionUpdate update{ active, duration, reason, reason, exclude_alts };

	const auto update_req = std::make_shared<UserRestrictionPatchUpdateV2Request>(api_key, opt_restriction->get_path(), update);
	OperationInProgressDialog diag{ this, update_req };
	diag.exec();
	if (update_req->req_success()) pressed_refresh();
}
