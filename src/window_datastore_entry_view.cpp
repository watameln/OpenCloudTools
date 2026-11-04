#include "window_datastore_entry_view.h"

#include <algorithm>

#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "datastore_entry_payload.h"
#include "data_request.h"
#include "diag_confirm_change.h"
#include "diag_operation_in_progress.h"
#include "model_common.h"
#include "util_enum.h"

namespace
{
	class JsonHighlighter final : public QSyntaxHighlighter
	{
	public:
		explicit JsonHighlighter(QTextDocument* document) : QSyntaxHighlighter{ document } {}

	protected:
		void highlightBlock(const QString& text) override
		{
			static const QRegularExpression tokens{
				QStringLiteral(R"REGEX("(?:\\.|[^"\\])*"|\b(?:true|false|null)\b|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)REGEX")
			};
			auto matches = tokens.globalMatch(text);
			while (matches.hasNext())
			{
				const auto match = matches.next();
				const QString token = match.captured();
				QTextCharFormat format;
				if (token.startsWith('"'))
				{
					const bool is_key = text.mid(match.capturedEnd()).trimmed().startsWith(':');
					format.setForeground(QColor{ is_key ? "#9fb4d0" : "#bed1b3" });
				}
				else if (token == "true" || token == "false" || token == "null")
				{
					format.setForeground(QColor{ "#c7aad7" });
				}
				else
				{
					format.setForeground(QColor{ "#dbc29e" });
				}
				setFormat(match.capturedStart(), match.capturedLength(), format);
			}
		}
	};

	QLabel* selectable_label(const QString& text, QWidget* parent)
	{
		auto* label = new QLabel{ text, parent };
		label->setTextFormat(Qt::PlainText);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		return label;
	}

	void add_json_node(QTreeWidgetItem* parent, const QString& name, const QJsonValue& value,
		const int depth, int& remaining)
	{
		if (remaining <= 0)
		{
			return;
		}
		--remaining;
		auto* item = new QTreeWidgetItem{ parent };
		item->setText(0, name);
		if (value.isObject())
		{
			const QJsonObject obj = value.toObject();
			item->setText(1, QString{ "%1 properties" }.arg(obj.size()));
			item->setText(2, "Object");
			if (depth < 40)
			{
				for (auto iter = obj.constBegin(); iter != obj.constEnd() && remaining > 0; ++iter)
				{
					add_json_node(item, iter.key(), iter.value(), depth + 1, remaining);
				}
			}
		}
		else if (value.isArray())
		{
			const QJsonArray values = value.toArray();
			item->setText(1, QString{ "%1 items" }.arg(values.size()));
			item->setText(2, "Array");
			if (depth < 40)
			{
				for (int i = 0; i < values.size() && remaining > 0; ++i)
				{
					add_json_node(item, QString{ "[%1]" }.arg(i), values.at(i), depth + 1, remaining);
				}
			}
		}
		else
		{
			QString display;
			if (value.isString())
			{
				display = value.toString();
				item->setText(2, "String");
			}
			else if (value.isBool())
			{
				display = value.toBool() ? "true" : "false";
				item->setText(2, "Boolean");
			}
			else if (value.isDouble())
			{
				display = QString::number(value.toDouble(), 'g', 17);
				item->setText(2, "Number");
			}
			else
			{
				display = "null";
				item->setText(2, "Null");
			}
			display.replace('\n', QStringLiteral("\\n"));
			item->setToolTip(1, display);
			item->setText(1, display.size() > 320 ? display.left(320) + "..." : display);
		}
		item->setExpanded(depth < 2);
	}

	QPlainTextEdit* make_text_editor(QWidget* parent, const QString& value, bool writable, bool json)
	{
		auto* editor = new QPlainTextEdit{ parent };
		QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		font.setPointSize(10);
		editor->setFont(font);
		editor->setTabStopDistance(QFontMetricsF(font).horizontalAdvance(' ') * 4);
		editor->setLineWrapMode(QPlainTextEdit::NoWrap);
		editor->setReadOnly(!writable);
		editor->setStyleSheet("QPlainTextEdit { background: #17191c; border: 0; padding: 10px; }");
		editor->setPlainText(value);
		if (json)
		{
			new JsonHighlighter{ editor->document() };
		}
		return editor;
	}
}

ViewDatastoreEntryWindow::ViewDatastoreEntryWindow(
	QWidget* parent, const QString& key, const StandardDatastoreEntryFull& details,
	ViewEditMode view_edit_mode, bool allow_edit)
	: QWidget{ parent, Qt::Window },
	  data_type{ details.get_entry_type() },
	  edit_mode{ view_edit_mode == ViewEditMode::Edit },
	  api_key{ key },
	  universe_id{ details.get_universe_id() },
	  datastore_name{ details.get_datastore_name() },
	  scope{ details.get_scope() },
	  key_name{ details.get_key_name() },
	  version{ details.get_version() }
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowModality(Qt::ApplicationModal);
	setWindowTitle(edit_mode ? "Edit DataStore Entry - Open Cloud Tools" : "View DataStore Entry - Open Cloud Tools");
	setMinimumSize(690, 460);
	resize(1000, 680);

	initial_value = details.get_data_decoded();
	initial_user_ids = details.get_userids().value_or(QString{});
	initial_attributes = details.get_attributes().value_or(QString{});
	if (data_type == DatastoreEntryType::Json)
	{
		if (const auto pretty = pretty_json_document(initial_value)) initial_value = *pretty;
	}
	if (const auto pretty = pretty_json_document(initial_user_ids)) initial_user_ids = *pretty;
	if (const auto pretty = pretty_json_document(initial_attributes)) initial_attributes = *pretty;

	validation_timer = new QTimer{ this };
	validation_timer->setInterval(160);
	validation_timer->setSingleShot(true);
	connect(validation_timer, &QTimer::timeout, this, &ViewDatastoreEntryWindow::refresh_status);

	auto* summary = new QFrame{ this };
	summary->setObjectName("entrySummary");
	summary->setStyleSheet("QFrame#entrySummary { background: #27292d; border: 1px solid #3e4146; border-radius: 4px; }");
	auto* summary_layout = new QVBoxLayout{ summary };
	summary_layout->setContentsMargins(16, 12, 16, 12);
	summary_layout->setSpacing(6);

	auto* title_row = new QHBoxLayout{};
	auto* title = new QLabel{ edit_mode ? "Edit entry" : "View entry", summary };
	QFont title_font = title->font();
	title_font.setPointSize(14);
	title_font.setBold(true);
	title->setFont(title_font);
	title_row->addWidget(title);
	title_row->addStretch();

	auto* type_badge = new QLabel{
		data_type == DatastoreEntryType::Json ? "JSON" : get_enum_string(data_type), summary
	};
	type_badge->setStyleSheet("QLabel { background: #393c42; color: #d5d9df; padding: 4px 10px; border-radius: 3px; }");
	title_row->addWidget(type_badge);
	summary_layout->addLayout(title_row);

	auto* key_row = new QHBoxLayout{};
	key_row->addWidget(selectable_label(key_name, summary), 1);
	auto* copy_key_button = new QPushButton{ "Copy key", summary };
	connect(copy_key_button, &QPushButton::clicked, this, [this]() {
		QGuiApplication::clipboard()->setText(key_name);
	});
	key_row->addWidget(copy_key_button);
	summary_layout->addLayout(key_row);

	auto* context = selectable_label(datastore_name + "  /  " + scope, summary);
	context->setStyleSheet("QLabel { color: #a9afb8; }");
	summary_layout->addWidget(context);

	auto* detail_toggle = new QToolButton{ summary };
	detail_toggle->setText("Show entry details");
	detail_toggle->setCheckable(true);
	detail_toggle->setArrowType(Qt::RightArrow);
	detail_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	summary_layout->addWidget(detail_toggle, 0, Qt::AlignLeft);

	auto* details_panel = new QWidget{ summary };
	auto* grid = new QGridLayout{ details_panel };
	grid->setContentsMargins(0, 6, 0, 0);
	grid->setHorizontalSpacing(20);
	grid->setVerticalSpacing(8);
	grid->addWidget(new QLabel{ "Universe", details_panel }, 0, 0);
	grid->addWidget(selectable_label(QString::number(universe_id), details_panel), 0, 1);
	grid->addWidget(new QLabel{ "DataStore", details_panel }, 1, 0);
	grid->addWidget(selectable_label(datastore_name, details_panel), 1, 1);
	grid->addWidget(new QLabel{ "Scope", details_panel }, 2, 0);
	grid->addWidget(selectable_label(scope, details_panel), 2, 1);
	grid->addWidget(new QLabel{ "Version", details_panel }, 3, 0);
	grid->addWidget(selectable_label(version, details_panel), 3, 1);
	details_panel->setVisible(false);
	summary_layout->addWidget(details_panel);
	connect(detail_toggle, &QToolButton::toggled, this, [detail_toggle, details_panel](bool opened) {
		details_panel->setVisible(opened);
		detail_toggle->setArrowType(opened ? Qt::DownArrow : Qt::RightArrow);
		detail_toggle->setText(opened ? "Hide entry details" : "Show entry details");
	});

	tabs = new QTabWidget{ this };
	tabs->setDocumentMode(true);
	if (data_type == DatastoreEntryType::Json)
	{
		structure_tree = new QTreeWidget{ tabs };
		structure_tree->setHeaderLabels({ "Field", "Value", "Type" });
		structure_tree->setAlternatingRowColors(true);
		structure_tree->setUniformRowHeights(true);
		structure_tree->setIndentation(20);
		structure_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
		structure_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
		structure_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
		structure_tree->setColumnWidth(0, 260);
		structure_tab = tabs->addTab(structure_tree, "Structure");
	}
	value_editor = make_text_editor(tabs, initial_value, edit_mode, data_type == DatastoreEntryType::Json);
	value_tab = tabs->addTab(value_editor, data_type == DatastoreEntryType::Json ? "JSON" : "Value");
	if (edit_mode || details.get_userids().has_value())
	{
		user_ids_editor = make_text_editor(tabs, initial_user_ids, edit_mode, true);
		user_ids_editor->setPlaceholderText("Optional: [12345, 67890]");
		user_ids_tab = tabs->addTab(user_ids_editor, "User IDs");
	}
	if (edit_mode || details.get_attributes().has_value())
	{
		attributes_editor = make_text_editor(tabs, initial_attributes, edit_mode, true);
		attributes_editor->setPlaceholderText("Optional: {\"source\":\"game\"}");
		attributes_tab = tabs->addTab(attributes_editor, "Attributes");
	}
	if (edit_mode)
	{
		tabs->setCurrentIndex(value_tab);
		for (QPlainTextEdit* editor : { value_editor, user_ids_editor, attributes_editor })
		{
			if (!editor) continue;
			connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
				if (save_button) save_button->setEnabled(false);
				validation_timer->start();
			});
		}
	}
	else if (structure_tab >= 0)
	{
		tabs->setCurrentIndex(structure_tab);
	}

	auto* toolbar = new QHBoxLayout{};
	toolbar->setSpacing(8);
	find_edit = new QLineEdit{ this };
	find_edit->setPlaceholderText("Find in text...");
	find_edit->setClearButtonEnabled(true);
	find_edit->setMaximumWidth(260);
	find_next_button = new QPushButton{ "Find next", this };
	connect(find_edit, &QLineEdit::returnPressed, this, &ViewDatastoreEntryWindow::find_next);
	connect(find_next_button, &QPushButton::clicked, this, &ViewDatastoreEntryWindow::find_next);
	toolbar->addWidget(find_edit);
	toolbar->addWidget(find_next_button);
	toolbar->addStretch();
	format_button = new QPushButton{ "Format JSON", this };
	connect(format_button, &QPushButton::clicked, this, &ViewDatastoreEntryWindow::format_current_tab);
	toolbar->addWidget(format_button);
	if (edit_mode)
	{
		reset_button = new QPushButton{ "Reset tab", this };
		connect(reset_button, &QPushButton::clicked, this, &ViewDatastoreEntryWindow::reset_current_tab);
		toolbar->addWidget(reset_button);
	}
	auto* copy_button = new QPushButton{ "Copy", this };
	connect(copy_button, &QPushButton::clicked, this, &ViewDatastoreEntryWindow::copy_current_tab);
	toolbar->addWidget(copy_button);

	auto* footer = new QHBoxLayout{};
	status_label = new QLabel{ this };
	status_label->setWordWrap(true);
	footer->addWidget(status_label, 1);
	if (!edit_mode && allow_edit && data_type != DatastoreEntryType::Error)
	{
		auto* edit_button = new QPushButton{ "Edit entry...", this };
		connect(edit_button, &QPushButton::clicked, this, [this, details]() {
			QWidget* owner = parentWidget();
			const QString key_copy = api_key;
			close();
			auto* edit_window = new ViewDatastoreEntryWindow{ owner, key_copy, details, ViewEditMode::Edit };
			edit_window->show();
		});
		footer->addWidget(edit_button);
	}
	if (edit_mode)
	{
		save_button = new QPushButton{ "Save changes", this };
		save_button->setDefault(true);
		connect(save_button, &QPushButton::clicked, this, &ViewDatastoreEntryWindow::pressed_save);
		footer->addWidget(save_button);
	}
	auto* close_button = new QPushButton{ edit_mode ? "Cancel" : "Close", this };
	connect(close_button, &QPushButton::clicked, this, &QWidget::close);
	footer->addWidget(close_button);

	auto* layout = new QVBoxLayout{ this };
	layout->setContentsMargins(16, 14, 16, 14);
	layout->setSpacing(10);
	layout->addWidget(summary);
	layout->addLayout(toolbar);
	layout->addWidget(tabs, 1);
	layout->addLayout(footer);

	connect(tabs, &QTabWidget::currentChanged, this, [this](int tab) {
		if (tab == structure_tab) refresh_structure();
		refresh_toolbar();
	});

	auto* find_shortcut = new QShortcut{ QKeySequence::Find, this };
	connect(find_shortcut, &QShortcut::activated, this, [this]() {
		if (tabs->currentIndex() == structure_tab) tabs->setCurrentIndex(value_tab);
		find_edit->setFocus();
		find_edit->selectAll();
	});
	if (edit_mode)
	{
		auto* save_shortcut = new QShortcut{ QKeySequence::Save, this };
		connect(save_shortcut, &QShortcut::activated, this, [this]() {
			if (save_button && save_button->isEnabled()) pressed_save();
		});
	}

	if (structure_tree) refresh_structure();
	refresh_toolbar();
	refresh_status();
}

QPlainTextEdit* ViewDatastoreEntryWindow::current_editor() const
{
	const int index = tabs->currentIndex();
	if (index == value_tab) return value_editor;
	if (index == user_ids_tab) return user_ids_editor;
	if (index == attributes_tab) return attributes_editor;
	return nullptr;
}

void ViewDatastoreEntryWindow::refresh_toolbar()
{
	const QPlainTextEdit* editor = current_editor();
	format_button->setEnabled(editor && (tabs->currentIndex() != value_tab || data_type == DatastoreEntryType::Json));
	find_edit->setEnabled(editor != nullptr);
	find_next_button->setEnabled(editor != nullptr);
	if (reset_button)
	{
		bool modified = false;
		if (editor == value_editor) modified = editor->toPlainText() != initial_value;
		else if (editor == user_ids_editor) modified = editor->toPlainText() != initial_user_ids;
		else if (editor == attributes_editor) modified = editor->toPlainText() != initial_attributes;
		reset_button->setEnabled(modified);
	}
}

bool ViewDatastoreEntryWindow::has_unsaved_changes() const
{
	if (!edit_mode) return false;
	return value_editor->toPlainText() != initial_value
		|| (user_ids_editor && user_ids_editor->toPlainText() != initial_user_ids)
		|| (attributes_editor && attributes_editor->toPlainText() != initial_attributes);
}

void ViewDatastoreEntryWindow::refresh_status()
{
	if (!edit_mode)
	{
		status_label->setStyleSheet("QLabel { color: #aeb3bb; }");
		status_label->setText(QString{ "Read-only  |  %1 characters" }.arg(value_editor->toPlainText().size()));
		return;
	}

	const DatastoreEntryPreparation validation = prepare_datastore_entry(
		data_type, value_editor->toPlainText(),
		user_ids_editor ? user_ids_editor->toPlainText() : QString{},
		attributes_editor ? attributes_editor->toPlainText() : QString{}
	);
	if (!validation.entry)
	{
		status_label->setStyleSheet("QLabel { color: #e2aaab; }");
		status_label->setText(validation.error);
		save_button->setEnabled(false);
	}
	else
	{
		const bool modified = has_unsaved_changes();
		status_label->setStyleSheet("QLabel { color: #aebfb1; }");
		status_label->setText(modified ? "Valid data  |  Unsaved changes" : "Valid data  |  No changes");
		save_button->setEnabled(modified);
	}
	refresh_toolbar();
}

void ViewDatastoreEntryWindow::refresh_structure()
{
	if (!structure_tree) return;
	structure_tree->setUpdatesEnabled(false);
	structure_tree->clear();
	const QJsonDocument doc = QJsonDocument::fromJson(value_editor->toPlainText().toUtf8());
	if (!doc.isObject() && !doc.isArray())
	{
		auto* error = new QTreeWidgetItem{ structure_tree };
		error->setText(0, "Invalid JSON");
		error->setText(1, "Switch to JSON to correct the value.");
	}
	else
	{
		int remaining = 5000;
		const QJsonValue root = doc.isObject() ? QJsonValue{ doc.object() } : QJsonValue{ doc.array() };
		add_json_node(structure_tree->invisibleRootItem(), "(root)", root, 0, remaining);
		if (remaining <= 0)
		{
			auto* limit = new QTreeWidgetItem{ structure_tree };
			limit->setText(0, "Display limit");
			limit->setText(1, "Showing the first 5,000 fields. Full content is available in JSON.");
		}
	}
	structure_tree->setUpdatesEnabled(true);
}

void ViewDatastoreEntryWindow::format_current_tab()
{
	QPlainTextEdit* const editor = current_editor();
	if (!editor) return;
	const auto pretty = pretty_json_document(editor->toPlainText());
	if (!pretty)
	{
		QMessageBox::warning(this, "Invalid JSON", "The selected tab does not contain a valid JSON object or array.");
		return;
	}
	editor->setPlainText(*pretty);
	refresh_status();
}

void ViewDatastoreEntryWindow::reset_current_tab()
{
	if (!edit_mode) return;
	QPlainTextEdit* const editor = current_editor();
	if (!editor) return;
	if (editor == value_editor) editor->setPlainText(initial_value);
	else if (editor == user_ids_editor) editor->setPlainText(initial_user_ids);
	else if (editor == attributes_editor) editor->setPlainText(initial_attributes);
	refresh_status();
}

void ViewDatastoreEntryWindow::copy_current_tab()
{
	QPlainTextEdit* const editor = current_editor();
	QGuiApplication::clipboard()->setText(editor ? editor->toPlainText() : value_editor->toPlainText());
}

void ViewDatastoreEntryWindow::find_next()
{
	QPlainTextEdit* editor = current_editor();
	if (!editor || find_edit->text().isEmpty()) return;
	if (!editor->find(find_edit->text()))
	{
		QTextCursor cursor = editor->textCursor();
		cursor.movePosition(QTextCursor::Start);
		editor->setTextCursor(cursor);
		editor->find(find_edit->text());
	}
}

void ViewDatastoreEntryWindow::select_invalid_field(const int field)
{
	if (field == 1 && user_ids_tab >= 0) tabs->setCurrentIndex(user_ids_tab);
	else if (field == 2 && attributes_tab >= 0) tabs->setCurrentIndex(attributes_tab);
	else tabs->setCurrentIndex(value_tab);
}

void ViewDatastoreEntryWindow::pressed_save()
{
	if (!edit_mode || !has_unsaved_changes()) return;
	const DatastoreEntryPreparation prepared = prepare_datastore_entry(
		data_type, value_editor->toPlainText(),
		user_ids_editor ? user_ids_editor->toPlainText() : QString{},
		attributes_editor ? attributes_editor->toPlainText() : QString{}
	);
	if (!prepared.entry)
	{
		select_invalid_field(prepared.invalid_field);
		QMessageBox::warning(this, "Invalid entry data", prepared.error);
		return;
	}

	ConfirmChangeDialog confirm{ this, ChangeType::StandardDatastoreUpdate };
	if (confirm.exec() != QDialog::Accepted) return;

	const auto request = std::make_shared<StandardDatastoreEntryPostSetRequest>(
		api_key, universe_id, datastore_name, scope, key_name,
		prepared.entry->user_ids, prepared.entry->attributes, prepared.entry->body
	);
	OperationInProgressDialog progress{ this, request };
	progress.exec();
	if (request->req_success())
	{
		saved = true;
		close();
	}
}

void ViewDatastoreEntryWindow::closeEvent(QCloseEvent* event)
{
	if (edit_mode && !saved && has_unsaved_changes())
	{
		const QMessageBox::StandardButton choice = QMessageBox::question(
			this, "Discard changes?",
			"This entry has unsaved changes. Discard them and close the editor?",
			QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel
		);
		if (choice != QMessageBox::Discard)
		{
			event->ignore();
			return;
		}
	}
	QWidget::closeEvent(event);
}
