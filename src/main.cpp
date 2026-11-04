#include <QApplication>
#include <QIcon>
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

	MyMainWindow* const window = new MyMainWindow{};
	window->show();

	return app.exec();
}
