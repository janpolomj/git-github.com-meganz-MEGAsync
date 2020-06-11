#ifndef MEGASPEEDGRAPH_H
#define MEGASPEEDGRAPH_H

#include <QWidget>
#include <QPainterPath>
#include "megaapi.h"

namespace Ui {
class MegaSpeedGraph;
}

class MegaSpeedGraph : public QWidget
{
    Q_OBJECT

public:
    explicit MegaSpeedGraph(QWidget *parent = 0);
    void init(mega::MegaApi *megaApi, int type, int numPoints = 10, int totalTimeMs = 10000);
    void start();
    void stop();
    ~MegaSpeedGraph();

private:
    Ui::MegaSpeedGraph *ui;
    mega::MegaApi *megaApi;
    int type;
    int numPoints;
    int totalTimeMs;
    QTimer *timer;
    QList<long long> values;
    QPolygonF polygon;
    QPainterPath linePath;
    QPainterPath closedLinePath;
    long long max;

    int radius;
    QColor gradientColor;
    QColor graphLineColor;
    QColor verticalLineColor;

protected:
    void clearValues();
    void paintEvent(QPaintEvent *event);

protected slots:
    void sample();

signals:
    void newValue(long long value);
};

#endif // MEGASPEEDGRAPH_H
