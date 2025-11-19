#pragma once
#include <QMainWindow>

class ClippingCanvas;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openSegmentsFile();
    void openPolygonFile();
    void clearScene();
    void showAbout();

private:
    ClippingCanvas *canvas = nullptr;

    void createMenus();
};
