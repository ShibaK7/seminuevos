#include "../include/mainwindow.h"

#include "vehicleitemlist.h"
#include "../ui/ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->inventoryButton->click();
    ui->vehicleList->setSpacing(0);


    QListWidget* listWidget = ui->vehicleList;

    for (int i = 0; i<10; i++)
    {
        VehicleItemList* card = new VehicleItemList();

        card->setModel("Seat Peugeot 208 Allure " + QString::number(i));
        card->setPrice("$140,000" + QString::number(i));
        card->setStatus("Disponible" + QString::number(i));
        card->setImage(QPixmap(":/resources/images/images/background.png"));
        card->setYear("2016" + QString::number(i));
        card->setColor("Negro" + QString::number(i));
        card->setKm("190,000" + QString::number(i));
        card->setTransmission("Automatico" + QString::number(i));
        card->setMotor("Diesel" + QString::number(i));
        card->setDate("10/10/2020" + QString::number(i));

        QListWidgetItem* itemNew = new QListWidgetItem(listWidget);
        itemNew->setSizeHint(card->sizeHint());
        listWidget->addItem(itemNew);
        listWidget->setItemWidget(itemNew, card);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_inventoryButton_clicked()
{
    ui->acquisitionButton->click();
}

void MainWindow::on_commercialButton_clicked()
{

}

void MainWindow::on_financeButton_clicked()
{

}

void MainWindow::on_reportButton_clicked()
{

}
