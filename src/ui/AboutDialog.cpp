#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

#include "BuildInfo.h" // generated at build time; see cmake/GenerateBuildInfo.cmake

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Hard Link Manager"));

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(QStringLiteral(":/icons/app/app256.png")));

    auto *nameLabel = new QLabel(tr("Hard Link Manager"), this);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 4);
    nameLabel->setFont(nameFont);

    auto *versionLabel = new QLabel(tr("v%1").arg(QStringLiteral(APP_VERSION)), this);
    versionLabel->setObjectName(QStringLiteral("ad.versionLabel"));

    auto *buildDateLabel = new QLabel(tr("Built %1").arg(QStringLiteral(APP_BUILD_DATE)), this);
    buildDateLabel->setObjectName(QStringLiteral("ad.buildDateLabel"));

    auto *copyrightLabel = new QLabel(tr("© Copyright 2026 Dan Brakeley"), this);

    auto *githubLabel = new QLabel(
        tr("<a href=\"https://github.com/danbrakeley/hardlinkmgr\">github.com/danbrakeley/hardlinkmgr</a>"),
        this);
    githubLabel->setObjectName(QStringLiteral("ad.githubLink"));
    githubLabel->setTextFormat(Qt::RichText);
    githubLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    githubLabel->setOpenExternalLinks(true);

    auto *textLayout = new QVBoxLayout;
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(versionLabel);
    textLayout->addWidget(buildDateLabel);
    textLayout->addSpacing(12);
    textLayout->addWidget(copyrightLabel);
    textLayout->addWidget(githubLabel);
    textLayout->addStretch(1);

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(iconLabel);
    topLayout->addLayout(textLayout, /*stretch*/ 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    buttons->setObjectName(QStringLiteral("ad.okButton"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(topLayout);
    layout->addWidget(buttons);
}
