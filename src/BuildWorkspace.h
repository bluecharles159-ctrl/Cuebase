#pragma once

#include <QString>
#include <QWidget>

class Library;
class QLabel;
class QLineEdit;
class QComboBox;
class BuildModel;
class BuildCanvas;

// The Build workspace: a 2D broadcast composition editor that can contain real 3D objects.
//   - fixed-camera artboard with 2D pan/zoom and 3D transforms
//   - compact inspector with explicit property key buttons and Advanced sections
//   - timeline plus an interactive graph editor
//   - mesh editing, modifiers, materials and world controls remain available
// Elements made here are saved into Screen Elements as "Custom" elements.
class BuildWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit BuildWorkspace(Library *lib, QWidget *parent = nullptr);
    void undoEdit();
    void redoEdit();
    bool hasUnsavedChanges() const;
    bool saveForClose();
    bool importFile(const QString &path);
    void exportFile();
    void newElement();                          // start from a fresh scene
    void newEffect();
    void editElement(const QString &elementId); // open a saved custom element

signals:
    void closeRequested();                      // "← Studio"

private:
    void save();
    void say(const QString &text, bool error = false);

    Library *m_lib;
    BuildModel *m_model;
    BuildCanvas *m_canvas;
    QLineEdit *m_name;
    QComboBox *m_elementPicker;
    void refreshElements();
    void capturePreview();
    QLabel *m_msg;
    QString m_editingId;
    QString m_savedJson, m_savedName, m_draftName;
};
