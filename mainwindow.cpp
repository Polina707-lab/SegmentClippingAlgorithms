#include "mainwindow.h"
#include "clippingcanvas.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    canvas = new ClippingCanvas(this);
    setCentralWidget(canvas);

    resize(1000, 800);
    setWindowTitle("Алгоритмы отсечения отрезков и многоугольников");

    // подгружаем стиль из ресурсов
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        const QString style = QString::fromUtf8(styleFile.readAll());
        qApp->setStyleSheet(style);
    }

    createMenus();

    connect(canvas, &ClippingCanvas::cursorGridPosChanged,
            this, [this](const QPointF &pt){
                statusBar()->showMessage(
                    QString("X: %1  Y: %2")
                        .arg(pt.x(), 0, 'f', 2)
                        .arg(pt.y(), 0, 'f', 2));
            });
}

void MainWindow::createMenus()
{
    // --- Файл ---
    QMenu *fileMenu = menuBar()->addMenu("Файл");

    QAction *openSegAct = fileMenu->addAction("Открыть отрезки...");
    connect(openSegAct, &QAction::triggered,
            this, &MainWindow::openSegmentsFile);

    QAction *openPolyAct = fileMenu->addAction("Открыть многоугольник...");
    connect(openPolyAct, &QAction::triggered,
            this, &MainWindow::openPolygonFile);

    fileMenu->addSeparator();

    QAction *clearAct = fileMenu->addAction("Очистить");
    connect(clearAct, &QAction::triggered,
            this, &MainWindow::clearScene);

    fileMenu->addSeparator();
    fileMenu->addAction("Выход", this, &QWidget::close);

    // --- Справка ---
    QMenu *helpMenu = menuBar()->addMenu("Справка");
    helpMenu->addAction("О программе", this, &MainWindow::showAbout);
}

void MainWindow::openSegmentsFile()
{
    QString fn = QFileDialog::getOpenFileName(
        this,
        "Открыть файл с отрезками",
        "/data",
        "Text files (*.txt);;All files (*.*)");

    if (fn.isEmpty())
        return;

    if (!canvas->loadSegmentsFromFile(fn)) {
        QMessageBox::warning(this, "Ошибка",
                             "Не удалось загрузить файл с отрезками.");
    }
}

void MainWindow::openPolygonFile()
{
    QString fn = QFileDialog::getOpenFileName(
        this,
        "Открыть файл с многоугольником",
        "/data",
        "Text files (*.txt);;All files (*.*)");

    if (fn.isEmpty())
        return;

    if (!canvas->loadPolygonFromFile(fn)) {
        QMessageBox::warning(this, "Ошибка",
                             "Не удалось загрузить файл с многоугольником.");
    }
}


void MainWindow::clearScene()
{
    canvas->clearAll();
}

void MainWindow::showAbout()
{
    QMessageBox::information(
        this,
        "О программе",
        "Алгоритмы отсечения отрезков и многоугольников\n\n"
        "Часть 1: алгоритм средней точки (Midpoint subdivision)\n"
        "Часть 2: алгоритм Сазерленда–Ходжмана для выпуклого многоугольника\n"
        "относительно прямоугольного окна.\n\n"
        "Вариант: 9 (отрезки — алгоритм средней точки; "
        "многоугольники — отсечение выпуклого многоугольника).\n\n"
        "Автор: Шамрук Полина Александровна, 1 курс, группа 11."
        );
}
