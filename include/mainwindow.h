#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QListWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // Left Bar Buttons
    void on_inventoryButton_clicked();
    void on_commercialButton_clicked();
    void on_financeButton_clicked();
    void on_reportButton_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H