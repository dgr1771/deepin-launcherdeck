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
        // 按行分组后逐行水平居中（对齐 win 版 justify-content:center——
        // 左对齐时不满行的空隙全堆右侧，左右不对称）
        struct Row { QList<QLayoutItem *> items; int width = 0; int height = 0; };
        QList<Row> rows;
        Row row;
        const int avail = rect.width();
        for (QLayoutItem *it : m_items) {
            const QSize ws = it->sizeHint();
            const int next = row.width + (row.items.isEmpty() ? 0 : m_h) + ws.width();
            if (!row.items.isEmpty() && next > avail) { rows.append(row); row = Row(); }
            row.width += (row.items.isEmpty() ? 0 : m_h) + ws.width();
            row.height = qMax(row.height, ws.height());
            row.items.append(it);
        }
        if (!row.items.isEmpty()) rows.append(row);

        int y = rect.y();
        for (const Row &r : rows) {
            int rx = rect.x() + (avail - r.width) / 2;   // 行居中：左右留白对称
            for (QLayoutItem *it : r.items) {
                const QSize ws = it->sizeHint();
                if (!test) it->setGeometry(QRect(QPoint(rx, y), ws));
                rx += ws.width() + m_h;
            }
            y += r.height + m_v;
        }
        return y - rect.y() - m_v + 4;
    }
    QList<QLayoutItem *> m_items;
    int m_h, m_v;
};
