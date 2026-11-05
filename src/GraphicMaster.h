#pragma once

#include <QString>
#include <functional>

class Library;
class QMenu;
class QWidget;

void populateGraphicMasters(QMenu *menu, Library *library, QWidget *parent,
                            std::function<bool()> saveDraft,
                            std::function<void(QString)> openVariant);
