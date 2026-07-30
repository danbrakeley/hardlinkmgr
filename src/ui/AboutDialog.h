#pragma once

#include <QDialog>

// Modal "About" dialog: app icon + name/version on top, copyright and a
// github link underneath, single OK button. Reached from the toolbar's
// About action (see MainWindow).
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
