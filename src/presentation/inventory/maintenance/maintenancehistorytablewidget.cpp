#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"
#include "ui_maintenancehistorytablewidget.h"

#include "presentation/common/forms/formsupport.h"

#include <QHeaderView>
#include <QPushButton>

MaintenanceHistoryTableWidget::MaintenanceHistoryTableWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MaintenanceHistoryTableWidget)
{
    // La tarjeta lleva class="card" en el .ui y no el nombre cardPanel: este
    // widget va promovido dentro del Paso 1, y las pruebas cuentan las dos
    // tarjetas cardPanel del paso. La sombra no cabe en el .ui: Designer no la
    // puede declarar.
    ui->setupUi(this);
    formsupport::applyFloatingShadow(ui->historyCard);

    // La descripción es lo único de largo libre: se lleva el ancho que sobre.
    // Las demás columnas se ajustan a su contenido.
    QHeaderView *header = ui->historyTable->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(DescriptionColumn, QHeaderView::Stretch);

    connect(ui->addMaintenanceButton, &QPushButton::clicked, this,
            &MaintenanceHistoryTableWidget::addMaintenanceRequested);
    connect(ui->printReceiptsButton, &QPushButton::clicked, this,
            &MaintenanceHistoryTableWidget::printReceiptsRequested);
}

MaintenanceHistoryTableWidget::~MaintenanceHistoryTableWidget()
{
    delete ui;
}

QList<QWidget *> MaintenanceHistoryTableWidget::focusOrder() const
{
    return {ui->addMaintenanceButton, ui->historyTable, ui->printReceiptsButton};
}
