#pragma once

#include <optional>

#include <QString>

#include "util_enum.h"

struct PreparedDatastoreEntry
{
	QString body;
	std::optional<QString> user_ids;
	std::optional<QString> attributes;
};

struct DatastoreEntryPreparation
{
	std::optional<PreparedDatastoreEntry> entry;
	QString error;
	int invalid_field = -1; // 0=value, 1=user IDs, 2=attributes
};

std::optional<QString> pretty_json_document(const QString& input);
DatastoreEntryPreparation prepare_datastore_entry(
	DatastoreEntryType type,
	const QString& value,
	const QString& user_ids,
	const QString& attributes
);
