#include "TestSupport.h"

#include <QAbstractButton>
#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>

namespace testsupport {

void autoAnswerNextMessageBox(QMessageBox::StandardButton button, int timeoutMs)
{
    auto *timer = new QTimer(qApp);
    QElapsedTimer elapsed;
    elapsed.start();
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, elapsed, button, timeoutMs] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (QAbstractButton *toClick = box->button(button)) {
                toClick->click();
            } else {
                box->reject(); // wrong expectation is better surfaced than a hang
            }
            timer->stop();
            timer->deleteLater();
            return;
        }
        if (elapsed.elapsed() > timeoutMs) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start(10);
}

} // namespace testsupport
