// 流式布局（Qt 官方 FlowLayout 范例的紧凑版）：牌阵网格随宽度自动换行
#pragma once
#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent, int hSpace = 13, int vSpace = 14)
        : QLayout(parent), m_h(hSpace), m_v(vSpace) { setContentsMargins(6, 4, 6, 14); }
    ~FlowLayout() override { QLayoutItem *it; while ((it = takeAt(0))) delete it; }

    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem *itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem *takeAt(int i) override { return i >= 0 && i < m_items.size() ? m_items.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    void setGeometry(const QRect &rect) override { doLayout(rect); QLayout::setGeometry(rect); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize s;
        for (QLayoutItem *it : m_items) s = s.expandedTo(it->minimumSize());
        return s + QSize(2 * 6, 2 * 4);
    }

private:
    int doLayout(const QRect &rect, bool test = false) const {
        int x = rect.x(), y = rect.y(), lineHeight = 0;
        for (QLayoutItem *it : m_items) {
            const QSize ws = it->sizeHint();
            int nextX = x + ws.width() + m_h;
            if (nextX - rect.right() > m_h && lineHeight > 0) { x = rect.x(); y += lineHeight + m_v; nextX = x + ws.width() + m_h; lineHeight = 0; }
            if (!test) it->setGeometry(QRect(QPoint(x, y), ws));
            x = nextX;
            lineHeight = qMax(lineHeight, ws.height());
        }
        return y + lineHeight - rect.y() + 4;
    }
    QList<QLayoutItem *> m_items;
    int m_h, m_v;
};
