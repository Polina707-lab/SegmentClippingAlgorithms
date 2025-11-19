#include "clippingcanvas.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <algorithm>
#include <QToolTip>


ClippingCanvas::ClippingCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(800, 600);
}

// ---------- координаты ----------
QPointF ClippingCanvas::originPx() const
{
    return QPointF(width() / 2.0, height() / 2.0);
}

QPointF ClippingCanvas::gridToScreenF(QPointF g) const
{
    // логические координаты: X вправо, Y вверх
    return originPx() + panPx + QPointF(g.x() * cellSize, -g.y() * cellSize);
}

QPoint ClippingCanvas::gridToScreen(QPoint g) const
{
    return gridToScreenF(QPointF(g)).toPoint();
}

QPointF ClippingCanvas::screenToGridF(QPointF s) const
{
    QPointF v = s - originPx() - panPx;
    return QPointF(v.x() / cellSize, -v.y() / cellSize);
}

QPointF ClippingCanvas::screenToGridF(QPoint s) const
{
    return screenToGridF(QPointF(s));
}

// ---------- загрузка данных ----------

bool ClippingCanvas::loadSegmentsFromFile(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);

    int n;
    in >> n;
    if (in.status() != QTextStream::Ok)
        return false;

    segmentsOriginal.clear();
    segmentsClipped.clear();
    polygonOriginal.clear();
    polygonClipped.clear();

    for (int i = 0; i < n; ++i)
    {
        double x1, y1, x2, y2;
        in >> x1 >> y1 >> x2 >> y2;
        if (in.status() != QTextStream::Ok)
            return false;

        segmentsOriginal.append(QLineF(QPointF(x1, y1), QPointF(x2, y2)));
    }

    double xmin, ymin, xmax, ymax;
    in >> xmin >> ymin >> xmax >> ymax;
    if (in.status() != QTextStream::Ok)
        return false;

    clipWindow = QRectF(QPointF(xmin, ymin), QPointF(xmax, ymax));
    hasWindow = true;
    currentMode = Mode::SegmentsMidpoint;

    clipAllSegmentsMidpoint();
    update();
    return true;
}

bool ClippingCanvas::loadPolygonFromFile(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);

    int n;
    in >> n;
    if (in.status() != QTextStream::Ok || n < 3)
        return false;

    polygonOriginal.clear();
    polygonClipped.clear();
    segmentsOriginal.clear();
    segmentsClipped.clear();

    for (int i = 0; i < n; ++i)
    {
        double x, y;
        in >> x >> y;
        if (in.status() != QTextStream::Ok)
            return false;

        polygonOriginal.append(QPointF(x, y));
    }

    double xmin, ymin, xmax, ymax;
    in >> xmin >> ymin >> xmax >> ymax;
    if (in.status() != QTextStream::Ok)
        return false;

    clipWindow = QRectF(QPointF(xmin, ymin), QPointF(xmax, ymax));
    hasWindow = true;
    currentMode = Mode::PolygonSuthHodg;

    clipPolygonSutherlandHodgman();
    update();
    return true;
}

void ClippingCanvas::clearAll()
{
    segmentsOriginal.clear();
    segmentsClipped.clear();
    polygonOriginal.clear();
    polygonClipped.clear();
    hasWindow = false;
    currentMode = Mode::None;
    update();
}

// ---------- логические проверки для алгоритмов ----------

bool ClippingCanvas::pointInside(const QPointF &P) const
{
    if (!hasWindow) return false;
    return (P.x() >= clipWindow.left()  &&
            P.x() <= clipWindow.right() &&
            P.y() >= clipWindow.top()   &&
            P.y() <= clipWindow.bottom());
}

// полностью вне окна: обе точки по одну сторону
bool ClippingCanvas::segOutside(const QPointF &A, const QPointF &B) const
{
    if (!hasWindow) return true;

    if (A.x() < clipWindow.left()  && B.x() < clipWindow.left())  return true;
    if (A.x() > clipWindow.right() && B.x() > clipWindow.right()) return true;
    if (A.y() < clipWindow.top()   && B.y() < clipWindow.top())   return true;
    if (A.y() > clipWindow.bottom()&& B.y() > clipWindow.bottom())return true;

    return false;
}

// ---------- Алгоритм средней точки ----------

void ClippingCanvas::clipMidpoint(const QPointF &A,
                                  const QPointF &B,
                                  QVector<QLineF> &outLines) const
{
    double dx = B.x() - A.x();
    double dy = B.y() - A.y();
    double len2 = dx * dx + dy * dy;

    if (len2 < 1e-3)
        return;

    if (segOutside(A, B))
        return;

    bool Ainside = pointInside(A);
    bool Binside = pointInside(B);

    // Оба inside → целиком видно
    if (Ainside && Binside) {
        outLines.append(QLineF(A, B));
        return;
    }

    // Середина
    QPointF M((A.x() + B.x()) / 2.0, (A.y() + B.y()) / 2.0);


    // Рекурсивное деление
    clipMidpoint(A, M, outLines);
    clipMidpoint(M, B, outLines);
}

void ClippingCanvas::clipAllSegmentsMidpoint()
{
    segmentsClipped.clear();
    intersectionPoints.clear();
    if (!hasWindow) return;

    QVector<QLineF> visible;

    for (const QLineF &s : std::as_const(segmentsOriginal)) {

        // --- добавляем реальные точки пересечения ---
        QVector<QPointF> realPts = findRealIntersections(s.p1(), s.p2());
        for (const QPointF &pt : realPts)
            intersectionPoints.append(pt);

        // --- запускаем midpoint ---
        clipMidpoint(s.p1(), s.p2(), visible);
    }

    segmentsClipped = visible;
}


// ---------- Сазерленд–Ходжман ----------

bool ClippingCanvas::insideEdge(const QPointF &P, Edge edge) const
{
    switch (edge) {
    case Edge::Left:   return P.x() >= clipWindow.left();
    case Edge::Right:  return P.x() <= clipWindow.right();
    case Edge::Bottom: return P.y() >= clipWindow.top();    // Y вверх
    case Edge::Top:    return P.y() <= clipWindow.bottom();
    }
    return false;
}

QPointF ClippingCanvas::intersectWithEdge(const QPointF &S,
                                          const QPointF &E,
                                          Edge edge) const
{
    double dx = E.x() - S.x();
    double dy = E.y() - S.y();
    double t = 0.0;

    switch (edge) {
    case Edge::Left: {
        double x = clipWindow.left();
        t = (dx == 0.0) ? 0.0 : (x - S.x()) / dx;
        return QPointF(x, S.y() + t * dy);
    }
    case Edge::Right: {
        double x = clipWindow.right();
        t = (dx == 0.0) ? 0.0 : (x - S.x()) / dx;
        return QPointF(x, S.y() + t * dy);
    }
    case Edge::Bottom: {
        double y = clipWindow.top();
        t = (dy == 0.0) ? 0.0 : (y - S.y()) / dy;
        return QPointF(S.x() + t * dx, y);
    }
    case Edge::Top: {
        double y = clipWindow.bottom();
        t = (dy == 0.0) ? 0.0 : (y - S.y()) / dy;
        return QPointF(S.x() + t * dx, y);
    }
    }
    return S;
}

QVector<QPointF> ClippingCanvas::clipAgainstEdge(const QVector<QPointF> &poly,
                                                 Edge edge)
{
    QVector<QPointF> out;
    if (poly.isEmpty())
        return out;

    const int n = poly.size();
    for (int i = 0; i < n; ++i) {
        QPointF S = poly[i];
        QPointF E = poly[(i + 1) % n];

        bool Sin = insideEdge(S, edge);
        bool Ein = insideEdge(E, edge);

        if (Sin && Ein) {
            // 1) внутри -> внутри: добавляем E
            out.append(E);
        } else if (Sin && !Ein) {
            // 2) внутри -> вне: добавляем точку пересечения
            QPointF I = intersectWithEdge(S, E, edge);
            intersectionPointsPolygon.append(I);
            out.append(I);
        } else if (!Sin && Ein) {
            // 3) вне -> внутри: добавляем пересечение и E
            QPointF I = intersectWithEdge(S, E, edge);
            intersectionPointsPolygon.append(I);
            out.append(I);
            out.append(E);
        } else {
            // 4) вне -> вне: ничего
        }
    }
    return out;
}

void ClippingCanvas::clipPolygonSutherlandHodgman()
{
    intersectionPointsPolygon.clear();
    polygonClipped = polygonOriginal;
    if (!hasWindow || polygonClipped.isEmpty())
        return;

    polygonClipped = clipAgainstEdge(polygonClipped, Edge::Left);
    polygonClipped = clipAgainstEdge(polygonClipped, Edge::Right);
    polygonClipped = clipAgainstEdge(polygonClipped, Edge::Bottom);
    polygonClipped = clipAgainstEdge(polygonClipped, Edge::Top);
}

// ---------- отрисовка ----------

void ClippingCanvas::drawGridAndAxes(QPainter &p)
{
    // фон
    p.fillRect(rect(), Qt::white);

    // ---------- Сетка ----------
    p.save();

    // логические границы области, которую видим
    QPointF gLT = screenToGridF(QPoint(0, 0));
    QPointF gRB = screenToGridF(QPoint(width(), height()));
    int gxMin = std::floor(std::min(gLT.x(), gRB.x()));
    int gxMax = std::ceil (std::max(gLT.x(), gRB.x()));
    int gyMin = std::floor(std::min(gLT.y(), gRB.y()));
    int gyMax = std::ceil (std::max(gLT.y(), gRB.y()));

    QColor fine(235, 235, 235);   // шаг 1
    QColor mid (210, 210, 210);   // шаг 5
    QColor bold(180, 180, 180);   // шаг 10

    // вертикальные линии
    for (int gx = gxMin; gx <= gxMax; ++gx) {
        QPointF s1 = gridToScreenF(QPointF(gx, gyMin));
        QPointF s2 = gridToScreenF(QPointF(gx, gyMax));

        if (gx % 10 == 0)
            p.setPen(QPen(bold, 1));
        else if (gx % 5 == 0)
            p.setPen(QPen(mid, 1));
        else
            p.setPen(QPen(fine, 1));

        p.drawLine(s1, s2);
    }

    // горизонтальные линии
    for (int gy = gyMin; gy <= gyMax; ++gy) {
        QPointF s1 = gridToScreenF(QPointF(gxMin, gy));
        QPointF s2 = gridToScreenF(QPointF(gxMax, gy));

        if (gy % 10 == 0)
            p.setPen(QPen(bold, 1));
        else if (gy % 5 == 0)
            p.setPen(QPen(mid, 1));
        else
            p.setPen(QPen(fine, 1));

        p.drawLine(s1, s2);
    }

    p.restore();

    // ---------- Оси ----------
    p.save();
    QPen axisPen(QColor(60, 60, 60), 2);
    p.setPen(axisPen);

    qreal ox = originPx().x() + panPx.x();
    qreal oy = originPx().y() + panPx.y();

    p.drawLine(QPointF(0, oy), QPointF(width(), oy));   // ось X
    p.drawLine(QPointF(ox, 0), QPointF(ox, height()));  // ось Y
    p.restore();

    // ---------- Подписи делений ----------
    p.save();
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    p.setPen(Qt::black);

    // шаг подписи, например каждые 1 или 2 логических единицы
    int labelStep = 2;
    if (cellSize < 20) labelStep = 5;

    // подписи по X
    for (int gx = gxMin; gx <= gxMax; gx += labelStep) {
        if (gx == 0) continue;
        QPointF pt = gridToScreenF(QPointF(gx, 0));
        p.drawText(pt.x() + 2, oy - 2, QString::number(gx));
    }

    // подписи по Y
    for (int gy = gyMin; gy <= gyMax; gy += labelStep) {
        QPointF pt = gridToScreenF(QPointF(0, gy));
        if (gy == 0) {
            p.drawText(pt.x() + 4, pt.y() - 2, "0");
        } else {
            p.drawText(ox + 4, pt.y() - 2, QString::number(gy));
        }
    }

    p.restore();
}


void ClippingCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    drawGridAndAxes(p);

    // --- окно отсечения ---
    if (hasWindow) {
        p.save();
        p.setPen(QPen(Qt::blue, 2));
        QPointF tl = gridToScreenF(clipWindow.topLeft());
        QPointF br = gridToScreenF(clipWindow.bottomRight());
        QRectF r(QPointF(std::min(tl.x(), br.x()),
                         std::min(tl.y(), br.y())),
                 QPointF(std::max(tl.x(), br.x()),
                         std::max(tl.y(), br.y())));
        p.drawRect(r);
        p.restore();
    }

    // --- режим: отрезки (Midpoint subdivision) ---
    if (currentMode == Mode::SegmentsMidpoint) {

        // исходные отрезки — пунктир, серые
        p.save();
        p.setPen(QPen(Qt::gray, 1, Qt::DashLine));
        for (const QLineF &s : std::as_const(segmentsOriginal)) {
            p.drawLine(gridToScreenF(s.p1()), gridToScreenF(s.p2()));
        }
        p.restore();

        // видимые части — красные
        p.save();
        p.setPen(QPen(Qt::red, 2));
        for (const QLineF &s : std::as_const(segmentsClipped)) {
            p.drawLine(gridToScreenF(s.p1()), gridToScreenF(s.p2()));
        }
        p.restore();
    }

    // --- режим: многоугольники (Sutherland–Hodgman) ---
    if (currentMode == Mode::PolygonSuthHodg) {

        // исходный многоугольник — красный пунктир
        p.save();
        p.setPen(QPen(QColor(200, 80, 80), 2, Qt::DashLine));
        if (!polygonOriginal.isEmpty()) {
            QPainterPath path;
            path.moveTo(gridToScreenF(polygonOriginal[0]));
            for (int i = 1; i < polygonOriginal.size(); ++i)
                path.lineTo(gridToScreenF(polygonOriginal[i]));
            path.closeSubpath();
            p.drawPath(path);
        }
        p.restore();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(QColor(120, 150, 255, 200)); // нежно-синий
        p.setPen(Qt::NoPen);

        for (const QPointF &pt : intersectionPointsPolygon) {
            QPointF S = gridToScreenF(pt);
            p.drawEllipse(S, 5, 5);
        }
        p.restore();

        // отсечённый — зелёная заливка
        p.save();
        p.setPen(QPen(QColor(0, 150, 0), 3));
        p.setBrush(QColor(0, 150, 0, 40));
        if (!polygonClipped.isEmpty()) {
            QPainterPath path;
            path.moveTo(gridToScreenF(polygonClipped[0]));
            for (int i = 1; i < polygonClipped.size(); ++i)
                path.lineTo(gridToScreenF(polygonClipped[i]));
            path.closeSubpath();
            p.drawPath(path);
        }
        p.restore();
    }

    // --- точки пересечения (только для Midpoint) ---
    if (currentMode == Mode::SegmentsMidpoint) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(QColor(255, 120, 120, 180));  // мягкий красный
        p.setPen(Qt::NoPen);

        for (const QPointF &pt : intersectionPoints) {
            QPointF S = gridToScreenF(pt);
            p.drawEllipse(S, 5, 5); // аккуратный кружочек
        }

        p.restore();
    }
}



// ---------- взаимодействие мышью / зум ----------

void ClippingCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (panning) {
        panPx += (e->pos() - lastMouse);
        lastMouse = e->pos();
        update();
        return;
    }

    QPointF g = screenToGridF(e->pos());
    bool hovering = false;

    // --- точки пересечения отрезков ---
    for (const QPointF &pt : intersectionPoints) {
        QPointF S = gridToScreenF(pt);
        if (QLineF(S, e->pos()).length() < 8) {

            QToolTip::showText(
                e->globalPosition().toPoint(),
                QString("(%1, %2)").arg(pt.x(), 0, 'f', 2)
                    .arg(pt.y(), 0, 'f', 2),
                this
                );

            hovering = true;
            break;
        }
    }

    // --- если не нашли в отрезках → проверяем многоугольник ---
    if (!hovering) {
        for (const QPointF &pt : intersectionPointsPolygon) {
            QPointF S = gridToScreenF(pt);
            if (QLineF(S, e->pos()).length() < 8) {

                QToolTip::showText(
                    e->globalPosition().toPoint(),
                    QString("(%1, %2)").arg(pt.x(), 0, 'f', 2)
                        .arg(pt.y(), 0, 'f', 2),
                    this
                    );

                hovering = true;
                break;
            }
        }
    }

    // --- если ни одна точка не подсвечена → скрыть tooltip ---
    if (!hovering)
        QToolTip::hideText();

    emit cursorGridPosChanged(g);
}



void ClippingCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton) {
        panning = true;
        lastMouse = e->pos();
    }
}

void ClippingCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton)
        panning = false;
}

void ClippingCanvas::wheelEvent(QWheelEvent *e)
{
    QPointF s = e->position();
    QPointF gBefore = screenToGridF(s);

    double factor = (e->angleDelta().y() > 0) ? 1.1 : 0.9;
    cellSize = std::clamp(cellSize * factor, 4.0, 80.0);

    QPointF desiredScreen =
        originPx() + panPx +
        QPointF(gBefore.x() * cellSize, -gBefore.y() * cellSize);
    panPx += (s - desiredScreen);

    update();
}


QVector<QPointF> ClippingCanvas::findRealIntersections(const QPointF &A, const QPointF &B)
{
    QVector<QPointF> pts;

    auto intersect = [&](const QPointF &P1, const QPointF &P2,
                         const QPointF &Q1, const QPointF &Q2,
                         QPointF &R) -> bool
    {
        QLineF line1(P1, P2);
        QLineF line2(Q1, Q2);
        QPointF ip;
        if (line1.intersects(line2, &ip) == QLineF::BoundedIntersection) {
            R = ip;
            return true;
        }
        return false;
    };

    QPointF R;

    // левая грань
    if (intersect(A, B,
                  QPointF(clipWindow.left(), clipWindow.top()),
                  QPointF(clipWindow.left(), clipWindow.bottom()), R))
        pts.append(R);

    // правая грань
    if (intersect(A, B,
                  QPointF(clipWindow.right(), clipWindow.top()),
                  QPointF(clipWindow.right(), clipWindow.bottom()), R))
        pts.append(R);

    // верхняя грань
    if (intersect(A, B,
                  QPointF(clipWindow.left(), clipWindow.top()),
                  QPointF(clipWindow.right(), clipWindow.top()), R))
        pts.append(R);

    // нижняя грань
    if (intersect(A, B,
                  QPointF(clipWindow.left(), clipWindow.bottom()),
                  QPointF(clipWindow.right(), clipWindow.bottom()), R))
        pts.append(R);

    return pts;
}
