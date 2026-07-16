#include <QApplication>

#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // libsmb2 leaves Winsock startup to the application (its own samples call
    // WSAStartup), so initialize it before any SMB connection is attempted.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return 1;
    }
#endif

    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
