#include <QApplication>
#include <QIcon>
#include <QTabWidget>

#include "model_common.h"
#include "window_datastore_entry_view.h"
#include <QStringList>

#include "window_main.h"

int main(int argc, char** argv)
{
	QApplication::setApplicationName("RobloxCloudManager");
	QApplication::setOrganizationName("RobloxCloudManager");

	QApplication app{ argc, argv };
	QApplication::setApplicationDisplayName("Open Cloud Tools");
	QApplication::setWindowIcon(QIcon{ ":/app/icon.ico" });

	if (app.arguments().contains("--smoke-test"))
	{
		return 0;
	}

	if (app.arguments().contains("--ui-smoke-test"))
	{
		QWidget owner;
		const StandardDatastoreEntryFull sample{
			123456, "SampleDataStore", "global", "SampleKey", "1",
			std::optional<QString>{ "[1,2]" },
			std::optional<QString>{ "{\"test\":true}" },
			"{\"nested\":{\"points\":120}}"
		};
		ViewDatastoreEntryWindow viewer{ &owner, "", sample, ViewEditMode::View };
		ViewDatastoreEntryWindow editor{ &owner, "", sample, ViewEditMode::Edit };
		return viewer.findChild<QTabWidget*>() && editor.findChild<QTabWidget*>() ? 0 : 1;
	}

	MyMainWindow* const window = new MyMainWindow{};
	window->show();

	return app.exec();
}
