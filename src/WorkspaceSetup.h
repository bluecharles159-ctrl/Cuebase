#pragma once
class Library;
class QDialog;
class QWidget;

namespace WorkspaceSetup {
bool editingOnly(const Library &library);
QDialog *dialog(QWidget *parent,Library *library);
}
