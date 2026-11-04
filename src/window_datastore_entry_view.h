#pragma once

#include <QString>
#include <QWidget>

#include "util_enum.h"

class QCloseEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTimer;
class QTreeWidget;
class StandardDatastoreEntryFull;

class ViewDatastoreEntryWindow : public QWidget
{
	Q_OBJECT

public:
	ViewDatastoreEntryWindow(QWidget* parent, const QString& api_key, const StandardDatastoreEntryFull& details,
		ViewEditMode view_edit_mode = ViewEditMode::View, bool allow_edit = false);

protected:
	void closeEvent(QCloseEvent* event) override;

private:
	void pressed_save();
	void refresh_status();
	void refresh_toolbar();
	void refresh_structure();
	void format_current_tab();
	void reset_current_tab();
	void copy_current_tab();
	void find_next();

	bool has_unsaved_changes() const;
	QPlainTextEdit* current_editor() const;
	void select_invalid_field(int field);

	DatastoreEntryType data_type;
	bool edit_mode = false;
	bool saved = false;

	QString api_key;
	long long universe_id = 0;
	QString datastore_name;
	QString scope;
	QString key_name;
	QString version;

	QString initial_value;
	QString initial_user_ids;
	QString initial_attributes;

	QTabWidget* tabs = nullptr;
	QTreeWidget* structure_tree = nullptr;
	QPlainTextEdit* value_editor = nullptr;
	QPlainTextEdit* user_ids_editor = nullptr;
	QPlainTextEdit* attributes_editor = nullptr;
	QLineEdit* find_edit = nullptr;
	QLabel* status_label = nullptr;
	QPushButton* save_button = nullptr;
	QPushButton* format_button = nullptr;
	QPushButton* reset_button = nullptr;
	QPushButton* find_next_button = nullptr;
	QTimer* validation_timer = nullptr;

	int structure_tab = -1;
	int value_tab = -1;
	int user_ids_tab = -1;
	int attributes_tab = -1;
};
