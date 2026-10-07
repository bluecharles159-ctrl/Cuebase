#pragma once
#include <QWidget>
#include <functional>
#include <QVector>
class BuildModel;
class Library;
class QVBoxLayout;
class QFormLayout;
class QDoubleSpinBox;
class PropertyInspector : public QWidget {
public:
    PropertyInspector(BuildModel *model, Library *library, QWidget *parent = nullptr);
private:
    void rebuild();
    void refresh();
    QFormLayout *section(const QString &title, bool open = true);
    void number(QFormLayout *form, const QString &label, const QString &path, double low, double high, double step = 1, const QString &suffix = {});
    void choice(QFormLayout *form, const QString &label, const QString &path, const QStringList &options, const QString &fallback);
    void color(QFormLayout *form, const QString &label, const QString &path, const QString &fallback);
    void apply(const std::function<void()> &operation);
    BuildModel *m;
    Library *lib;
    QVBoxLayout *layout;
    QVector<std::function<void()>> sync;
    QString signature;
    bool syncing = false;
};
