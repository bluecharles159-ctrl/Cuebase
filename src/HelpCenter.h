#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <functional>

class QWidget;

namespace HelpCenter {

void showWelcome(QWidget *parent, const std::function<void()> &startWalkthrough,
                 const std::function<void()> &showShortcuts);
void showWalkthrough(QWidget *parent);
void showReleaseNotes(QWidget *parent);
void showShortcuts(QWidget *parent, const QList<QPair<QString, QString>> &rows);
void showLicense(QWidget *parent);
void showPrivacy(QWidget *parent);
void showAbout(QWidget *parent);
void showDocumentation(QWidget *parent);
void openPlayground(QWidget *parent);
void openLink(QWidget *parent, const QString &what, const char *url);

} // namespace HelpCenter
