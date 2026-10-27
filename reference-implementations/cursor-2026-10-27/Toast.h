#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QTimer;

class ToastHost : public QWidget {
    Q_OBJECT
public:
    explicit ToastHost(QWidget *parent = nullptr);
    void showNotice(const QString &kind, const QString &message, const QString &detail = QString());
    static void notify(QWidget *context, const QString &kind, const QString &message, const QString &detail = QString());

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void layoutToast();
    QWidget *m_card = nullptr;
    QLabel *m_icon = nullptr;
    QLabel *m_text = nullptr;
    QTimer *m_hide = nullptr;
};
