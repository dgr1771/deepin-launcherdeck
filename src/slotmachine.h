#pragma once

#include <QWidget>
#include <QVector>
#include <QtGlobal>

class QTimer;

class SlotMachineBoard : public QWidget {
    Q_OBJECT
public:
    explicit SlotMachineBoard(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(760, 480); }

signals:
    void resultChanged(const QString &message, int wins);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    void spin();
    void finishSpin();
    void updateCooldown();

    QStringList m_symbols;
    QStringList m_reels;
    QVector<bool> m_winCols;
    QTimer *m_timer = nullptr;
    QTimer *m_fxTimer = nullptr;
    int m_ticks = 0;
    int m_wins = 0;
    bool m_spinning = false;
    int m_coins = 1000;
    static constexpr int SPIN_COST = 50;
    int m_spinsLeft = 20;
    qint64 m_resetAt = 0;
    bool m_jackpot = false;
    int m_fxTicks = 0;
    QString m_message = QStringLiteral("准备好了吗？今天的好运不设上限");
    QString m_winTitle;
};
