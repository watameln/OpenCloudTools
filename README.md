# Open Cloud Tools

Open Cloud Tools is a desktop application for Windows and MacOS that allows you to interact with Roblox's [Open Cloud](https://create.roblox.com/docs/cloud/open-cloud) API. Open Cloud Tools supports the Datastore, Ordered Datastore, and MessagingService APIs.

Icon by Roblox user [Loominatrx](https://devforum.roblox.com/u/loominatrx/summary).

## Screenshots

![Search Datastore](./extra/img/datastores_search.png)
![View Versions](./extra/img/datastore_entry_versions.png)
![Messaging](./extra/img/messaging.png)

## Features

### Memory Store

Sorted maps are currently the only supported type of memory store data. Queue support coming soon.

#### Sorted Maps

Read data in order from a [Memory Store Sorted Map](https://create.roblox.com/docs/cloud-services/memory-stores/sorted-map). Write support coming soon.

### Messaging Service

Send messages that your game servers can consume using [MessagingService](https://create.roblox.com/docs/cloud-services/cross-server-messaging).

### Datastore Operations

Store and retrieve data using Roblox's [Datastores](https://create.roblox.com/docs/cloud-services/datastores).

#### Basic Datastore Operations

* List datastores
* List datastore entries
* View entries
* View entry version history
  * View or revert to old versions
* Edit entries
* Delete entries

#### Advanced Datastore Operations

* [Bulk Download](./doc/bulk_download.md)
  * Dump all of the entries in one or more datastores to a sqlite database. This data can later be uploaded through the 'Bulk Upload' operation.
  * Large downloads can be stopped and resumed later.
* Bulk Delete
  * Delete all of the entries in one or more datastores.
* Bulk Undelete
  * Scan one or more datastores for deleted entries and restore their previous version.
* Bulk Upload
  * Upload a sqlite datastore dump. This can be used to restore from a backup or transfer data from one universe to another.

### Ordered Datastore Operations

Retrive data using Roblox's [Datastores](https://create.roblox.com/docs/cloud-services/datastores).

* List all keys in ascending or descending order.
* Write support coming Soon™.

## Creating an API Key

To create an API key, go to the [Open Cloud API Keys page of the Roblox Creator Dashboard](https://create.roblox.com/dashboard/credentials) and click the "Create API Key" button.

### Memory Store

No top-level memory store permissions are supported yet.

#### Sorted Map

These are self-explanatory:
* `memory-store.sorted-map:read`
* `memory-store.sorted-map:write`

OpenCloudTools does not currently use the `write` permission for anything.

### Messaging Permissions (messaging-service)

The only permission available for Messaging is `publish`, which is required to send messages.

### Datastore Permissions (universe-datastores)

The bare minimum permissions that the key will need to list and view entries are:
* `universe-datastores.control:list` - List datastores
* `universe-datastores.objects:list` - List entries
* `universe-datastores.objects:read` - Read entries

A read-only key can also optionally include:
* `universe-datastores.versions:list` - List versions
* `universe-datastores.versions:read` - Read version

The remaining permissions all allow modifying the datastore and you should give them out as needed:
* `universe-datastores.control:create` - Create datastore
* `universe-datastores.objects:create` - Create entry
* `universe-datastores.objects:update` - Update entry
* `universe-datastores.objects:delete` - Delete entry

### Ordered Datastore Permissions (ordered-data-stores)

The only permissions that exist for Ordered Datastores are:
* `universe.ordered-data-store.scope.entry:read`
* `universe.ordered-data-store.scope.entry:write`

## Building
## Windows portable build

Download the `OpenCloudTools-Windows-x64` artifact from the latest successful [Windows portable workflow](https://github.com/watameln/OpenCloudTools/actions/workflows/windows-portable.yml). Extract its inner ZIP, then run `OpenCloudTools/OpenCloudTools.exe`. Keep the provided `platforms` directory and DLLs alongside the executable. No separate Qt installation is required.

The workflow builds Qt 6.8.3 with MinGW 13.1 and verifies that the portable executable loads the Qt platform plugin. API functionality still requires your own valid Roblox API key.

To build locally, install Qt 6.8.3 for MinGW 13.1, the matching MinGW compiler, CMake and Ninja, and put Qt and MinGW `bin` directories on your PATH:

```powershell
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64"
cmake --build build/windows --parallel
windeployqt --release --compiler-runtime build/windows/OpenCloudTools.exe
```

Change the Qt installation path to your environment. The old `CMakeUserPresets.json` was tracked with a hard-coded `D:/Dev` path; it has been removed and is now local-only.

**Security:** API keys saved to disk are stored by Qt settings without encryption. Do not save sensitive keys on shared machines.

### API compatibility

Data store discovery uses the stable Open Cloud v2 `data-stores` list endpoint and reads the v2 `dataStores` / `nextPageToken` response. Empty lists no longer get treated as errors. Other standard data store operations still use v1 for compatibility with the existing bulk/restore implementation. Error handling now understands HTTP 201/202 and observes numeric `Retry-After` for rate limiting.

Current APIs are documented in [Roblox's Data and memory stores reference](https://create.roblox.com/docs/cloud/reference/features/storage). Bulk backup and legacy version handling have different requirements from basic v2 entry requests; their remaining v1 dependencies should be migrated separately and tested with a non-production universe.


This project uses CMake and requires a compiler that supports C++17.

CMake presets are provided in the repo, to use these just edit `CMakeUserPresets.json` and update the relevant `CMAKE_PREFIX_PATH` variables to point at your own Qt installation. If Qt is on your default path you can delete the `CMAKE_PREFIX_PATH` lines entirely or just use the *-raw presets which do not have a prefix path set.

### Dependencies

This project depends on SQLite and Qt 5/6. A recent version of SQLite is bundled in this repository, you have to provide your own Qt.

Instructions on how Qt is built for the binary distributions are available in [doc/building_qt.md](./doc/building_qt.md).

### Supported Platforms

This program can probably build and run on any platform that supports Qt 5 or 6, but has only been tested in the following configurations that are used to build the binary distributions:

| Platform    | Qt Version | Compiler            |
|-------------|------------|---------------------|
| MacOS       | 6.5.3      | XCode 13.2.1        |
| Windows Qt5 | 5.15.16    | Visual Studio 2022* |
| Windows Qt6 | 6.8.1      | Visual Studio 2022  |

\*For Qt5, OpenCloudTools itself is built with VS 2022, but Qt is built with VS 2019.

## License

This program is available under the GPLv3 License. See the [LICENSE](./LICENSE) file for the full text of this license.

## Tabbed workspace and DataStore key searching

The main content area uses full-size, closable tabs rather than overlapping child windows. Double-click a tool in the universe sidebar to open it; opening the same tool again focuses its existing tab. Ban List shows a searchable, sortable table with user IDs, dates, reasons, and actions.

The application now always uses **Fusion (Dark)**. Old saved theme preferences are ignored.

For a DataStore key prefix search, enter the DataStore name, optionally provide a scope (defaults to `global`), type the start of the key such as `Player_`, then select **Search prefix** or press Enter in the prefix field. The legacy list API may omit `scope` in its results; the parser now falls back to the requested scope instead of discarding such entries. CI includes regression tests for that behavior and pagination.
