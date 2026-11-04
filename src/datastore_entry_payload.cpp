#include "datastore_entry_payload.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "util_json.h"
#include "util_validator.h"

static std::optional<QJsonDocument> parse_document(const QString& text)
{
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || document.isNull())
	{
		return std::nullopt;
	}
	return document;
}

std::optional<QString> pretty_json_document(const QString& input)
{
	const std::optional<QJsonDocument> document = parse_document(input);
	if (!document)
	{
		return std::nullopt;
	}
	return QString::fromUtf8(document->toJson(QJsonDocument::Indented)).trimmed();
}

DatastoreEntryPreparation prepare_datastore_entry(
	const DatastoreEntryType type,
	const QString& value,
	const QString& user_ids,
	const QString& attributes)
{
	DatastoreEntryPreparation result;
	PreparedDatastoreEntry payload;
	if (type == DatastoreEntryType::Json)
	{
		QJsonParseError error;
		const QJsonDocument doc = QJsonDocument::fromJson(value.toUtf8(), &error);
		if (error.error != QJsonParseError::NoError || (!doc.isArray() && !doc.isObject()))
		{
			result.error = error.error == QJsonParseError::NoError
				? "Value must be a JSON object or array."
				: QString{ "Invalid JSON value: %1 (byte %2)" }.arg(error.errorString()).arg(error.offset);
			result.invalid_field = 0;
			return result;
		}
		payload.body = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
	}
	else if (type == DatastoreEntryType::String)
	{
		payload.body = encode_json_string(value);
	}
	else if (type == DatastoreEntryType::Bool)
	{
		const QString trimmed = value.trimmed();
		if (!DataValidator::is_bool(trimmed))
		{
			result.error = "Boolean value must be true or false.";
			result.invalid_field = 0;
			return result;
		}
		payload.body = trimmed;
	}
	else if (type == DatastoreEntryType::Number)
	{
		const QString trimmed = value.trimmed();
		if (!DataValidator::is_number(trimmed))
		{
			result.error = "Value must be a valid number.";
			result.invalid_field = 0;
			return result;
		}
		payload.body = trimmed;
	}
	else
	{
		result.error = "Unsupported entry type. It cannot be edited safely.";
		result.invalid_field = 0;
		return result;
	}

	const QString trimmed_users = user_ids.trimmed();
	if (!trimmed_users.isEmpty())
	{
		const std::optional<QJsonDocument> doc = parse_document(trimmed_users);
		if (!doc || !doc->isArray())
		{
			result.error = "User IDs must be a JSON array, such as [12345, 67890].";
			result.invalid_field = 1;
			return result;
		}
		payload.user_ids = QString::fromUtf8(doc->toJson(QJsonDocument::Compact));
	}

	const QString trimmed_attributes = attributes.trimmed();
	if (!trimmed_attributes.isEmpty())
	{
		const std::optional<QJsonDocument> doc = parse_document(trimmed_attributes);
		if (!doc || !doc->isObject())
		{
			result.error = "Attributes must be a JSON object, such as {\"source\":\"game\"}.";
			result.invalid_field = 2;
			return result;
		}
		payload.attributes = QString::fromUtf8(doc->toJson(QJsonDocument::Compact));
	}

	result.entry = std::move(payload);
	return result;
}
