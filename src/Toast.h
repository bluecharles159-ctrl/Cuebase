#pragma once
#include <QWidget>
class QLabel;
class QTimer;

// Adapted from the user's Cursor ToastHost prototype for the shared theme.
class ToastHost final : public QWidget {
    Q_OBJECT
public:
    explicit ToastHost(QWidget *parent);
    static void notify(QWidget *context,const QString &kind,const QString &message,const QString &detail={});
    void showNotice(const QString &kind,const QString &message,const QString &detail={});
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    void layoutToast();
    QWidget *m_card;
    QLabel *m_icon,*m_text;
    QTimer *m_hide;
    QString m_line;
};
