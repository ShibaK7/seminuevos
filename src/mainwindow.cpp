#include "../include/mainwindow.h"
#include "../ui/ui_mainwindow.h"
#include "../include/vehiclewizard/vehiclewizardview.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::openVehicleWizard);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openVehicleWizard()
{
    // Vista embebida (no modal): reemplaza la página de Inventario dentro
    // del mismo mainContentStack en vez de abrir un diálogo aparte.
    auto *wizard = new VehicleWizardView(ui->mainContentStack);

    connect(wizard, &VehicleWizardView::cancelled, this, &MainWindow::closeVehicleWizard);
    connect(wizard, &VehicleWizardView::vehicleRegistered, this, [this](int) { closeVehicleWizard(); });

    ui->mainContentStack->addWidget(wizard);
    ui->mainContentStack->setCurrentWidget(wizard);
}

void MainWindow::closeVehicleWizard()
{
    QWidget *current = ui->mainContentStack->currentWidget();
    if (current == ui->inventoryPage)
        return;

    ui->mainContentStack->setCurrentWidget(ui->inventoryPage);
    ui->mainContentStack->removeWidget(current);
    current->deleteLater();
}
