#pragma once

#include <QMessageBox>

namespace testsupport {

// Arms a zero-ish-interval poller that clicks the given standard button on
// the next modal QMessageBox to appear (the poller runs inside the box's
// nested event loop, so this works with the offscreen platform). Call BEFORE
// the action that opens the box. Gives up silently after timeoutMs.
void autoAnswerNextMessageBox(QMessageBox::StandardButton button, int timeoutMs = 5000);

} // namespace testsupport
