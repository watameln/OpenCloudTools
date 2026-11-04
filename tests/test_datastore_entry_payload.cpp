#include "datastore_entry_payload.h"

#include <iostream>

static int fail(const char* test)
{
	std::cerr << test << '\n';
	return 1;
}

int main()
{
	const auto json = prepare_datastore_entry(
		DatastoreEntryType::Json, " { \"amount\" : 15, \"flags\" : [true, false] } ",
		" [1, 2] ", " { \"source\": \"test\" } "
	);
	if (!json.entry) return fail("Valid JSON entry rejected");
	if (json.entry->body != "{\"amount\":15,\"flags\":[true,false]}") return fail("JSON formatting changed");
	if (json.entry->user_ids != std::optional<QString>{ "[1,2]" }) return fail("User ID array not condensed");
	if (json.entry->attributes != std::optional<QString>{ "{\"source\":\"test\"}" }) return fail("Attributes not condensed");

	const auto no_meta = prepare_datastore_entry(DatastoreEntryType::String, "a \"quoted\" value\n", "", "");
	if (!no_meta.entry || no_meta.entry->body != "\"a \\\"quoted\\\" value\\n\"")
		return fail("String payload escaped incorrectly");
	if (no_meta.entry->user_ids || no_meta.entry->attributes) return fail("Empty metadata not optional");

	const auto invalid_json = prepare_datastore_entry(DatastoreEntryType::Json, "{\"open\":", "", "");
	if (invalid_json.entry || invalid_json.invalid_field != 0) return fail("Malformed JSON accepted");

	const auto invalid_users = prepare_datastore_entry(DatastoreEntryType::Json, "{}", "{ }", "");
	if (invalid_users.entry || invalid_users.invalid_field != 1) return fail("Object used as user ID array");

	const auto invalid_attrs = prepare_datastore_entry(DatastoreEntryType::Json, "{}", "", "[]");
	if (invalid_attrs.entry || invalid_attrs.invalid_field != 2) return fail("Array used as attributes");

	const auto number = prepare_datastore_entry(DatastoreEntryType::Number, " 500 ", "", "");
	if (!number.entry || number.entry->body != "500") return fail("Number handling incorrect");

	const auto boolean = prepare_datastore_entry(DatastoreEntryType::Bool, " true ", "", "");
	if (!boolean.entry || boolean.entry->body != "true") return fail("Boolean handling incorrect");

	const auto unsupported = prepare_datastore_entry(DatastoreEntryType::Error, "x", "", "");
	if (unsupported.entry) return fail("Unsupported type accepted");

	const auto pretty = pretty_json_document("{\"one\":{\"two\":2}}");
	if (!pretty || !pretty->contains('\n')) return fail("Pretty JSON formatting failed");

	std::cout << "DataStore entry payload tests passed\n";
	return 0;
}
