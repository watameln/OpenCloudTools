#include "model_api_opencloud.h"
#include <QString>
#include <iostream>
static int fail(const char* msg) { std::cerr << msg << '\n'; return 1; }
int main()
{
	const QString payload = R"JSON({"keys":[{"key":"Player_123"},{"key":"Player_456","scope":"season1"}],"nextPageCursor":"second-page"})JSON";
	const auto parsed = GetStandardDatastoreEntryListResponse::from_json(payload, 1001, "Profiles", "global");
	if (!parsed) return fail("Valid response could not be parsed");
	if (parsed->get_entries().size() != 2) return fail("Response lost keys without an explicit scope");
	if (parsed->get_entries()[0].get_key() != "Player_123") return fail("Key parsed incorrectly");
	if (parsed->get_entries()[0].get_scope() != "global") return fail("Default scope missing");
	if (parsed->get_entries()[1].get_scope() != "season1") return fail("Explicit scope overwritten");
	if (parsed->get_cursor() != std::optional<QString>{"second-page"}) return fail("Pagination cursor not parsed");
	const auto scoped = GetStandardDatastoreEntryListResponse::from_json(R"JSON({"keys":[{"key":"Player_987"}]})JSON", 1001, "Profiles", "event");
	if (!scoped || scoped->get_entries().size() != 1 || scoped->get_entries()[0].get_scope() != "event") return fail("Requested scope fallback failed");
	const auto empty = GetStandardDatastoreEntryListResponse::from_json(R"JSON({"keys":[]})JSON", 1001, "Profiles", "global");
	if (!empty || !empty->get_entries().empty()) return fail("Empty response should be valid");
	const auto invalid = GetStandardDatastoreEntryListResponse::from_json(R"JSON({"unexpected":[]})JSON", 1001, "Profiles");
	if (invalid) return fail("Invalid response accepted");
	std::cout << "DataStore entry list parser tests passed\n";
	return 0;
}
