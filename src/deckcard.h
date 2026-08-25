// 塔罗牌卡片：自绘（星夜渐变 + 金线内框 + 角饰 + 悬停浮起发光）
// 尺寸由外部 fitTarotCards 计算——卡片只负责画得好看
#pragma once
#include <QWidget>
#include <QVariantAnimation>
#include "appscanner.h"

class DeckCard : public QWidget {
    Q_OBJECT
public:
    explicit DeckCard(const AppEntry &a, QWidget *parent = nullptr);
    void setIconPixmap(const QPixmap &pix) { m_icon = pix; update(); }
    const AppEntry &entry() const { return m_app; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    AppEntry m_app;
    QPixmap m_icon;
    QVariantAnimation *m_lift = nullptr;   // 0..1 浮起动画
    qreal m_liftVal = 0.0;
};
