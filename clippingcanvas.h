#pragma once
#include <QWidget>
#include <QVector>
#include <QLineF>
#include <QRectF>

class ClippingCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit ClippingCanvas(QWidget *parent = nullptr);

    bool loadSegmentsFromFile(const QString &fileName);

    bool loadPolygonFromFile(const QString &fileName);

    void clearAll();

signals:
    void cursorGridPosChanged(const QPointF &logicalPos);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    // --- координаты / масштаб ---
    qreal  cellSize = 40.0;  // размер клетки в пикселях
    QPointF panPx {0, 0};    // сдвиг
    bool   panning = false;
    QPoint lastMouse;
    QVector<QPointF> intersectionPoints;
    QVector<QPointF> intersectionPointsPolygon;

    QVector<QPointF> findRealIntersections(const QPointF &A, const QPointF &B);

    QPointF originPx() const;
    QPointF gridToScreenF(QPointF g) const;
    QPoint  gridToScreen(QPoint g) const;
    QPointF screenToGridF(QPointF s) const;
    QPointF screenToGridF(QPoint s) const;

    // --- данные для отрезков ---
    QVector<QLineF> segmentsOriginal;
    QVector<QLineF> segmentsClipped;

    // --- данные для многоугольников ---
    QVector<QPointF> polygonOriginal;
    QVector<QPointF> polygonClipped;

    // --- окно отсечения ---
    QRectF clipWindow;
    bool   hasWindow = false;

    // --- режимы ---
    enum class Mode { None, SegmentsMidpoint, PolygonSuthHodg };
    Mode currentMode = Mode::None;

    // === Алгоритм средней точки (отрезки) ===
    void clipAllSegmentsMidpoint();
    void clipMidpoint(const QPointF &A,
                      const QPointF &B,
                      QVector<QLineF> &outLines) const;

    bool segOutside(const QPointF &A, const QPointF &B) const;
    bool pointInside(const QPointF &P) const;

    // === Сазерленд–Ходжман (многоугольник) ===
    void clipPolygonSutherlandHodgman();

    enum class Edge { Left, Right, Bottom, Top };
    QVector<QPointF> clipAgainstEdge(const QVector<QPointF> &poly,
                                     Edge edge);
    bool insideEdge(const QPointF &P, Edge edge) const;
    QPointF intersectWithEdge(const QPointF &S, const QPointF &E,
                              Edge edge) const;

    // вспомогательное
    void drawGridAndAxes(QPainter &p);
};
