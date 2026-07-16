#include <QApplication>
#include <QLabel>
#include <QMainWindow>

#include <smb2/smb2.h>      // must precede libsmb2.h: defines SMB2_GUID_SIZE, smb2_lease_key, etc.
#include <smb2/libsmb2.h>

// Skeleton entry point: brings up an empty Qt Widgets window and proves the
// patched libsmb2 links and runs by creating and tearing down a context. This
// is scaffolding to build the real UI (see README / CLAUDE.md) on top of.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    struct smb2_context *smb2 = smb2_init_context();
    const bool smbOk = (smb2 != nullptr);
    if (smb2) {
        smb2_destroy_context(smb2);
    }

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Hard Link Manager"));

    auto *label = new QLabel(smbOk
        ? QStringLiteral("Qt + libsmb2 are wired up.\nsmb2_init_context() succeeded.")
        : QStringLiteral("Qt is up, but smb2_init_context() returned null."));
    label->setAlignment(Qt::AlignCenter);
    label->setMargin(24);
    window.setCentralWidget(label);

    window.resize(480, 160);
    window.show();

    return app.exec();
}
