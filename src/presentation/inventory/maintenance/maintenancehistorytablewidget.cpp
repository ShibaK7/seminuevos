#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"
#include "ui_maintenancehistorytablewidget.h"

#include "presentation/common/forms/formsupport.h"
#include "presentation/inventory/maintenance/addmaintenancedialog.h"
#include "presentation/inventory/maintenance/maintenancehistorypresenter.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QHeaderView>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QTableWidgetItem>
#include <QUrl>

namespace {

// El id de vehicle_maintenance de cada renglón va en la celda de la fecha.
constexpr int kIdRole = Qt::UserRole;

QTableWidgetItem *cell(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

} // namespace

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

    // La edición de una descripción termina al confirmar la celda (Enter o al
    // salir de ella). El aviso sale en la siguiente vuelta del bucle de
    // eventos: la tabla todavía está cerrando el editor, y la confirmación es
    // un diálogo modal que puede terminar repintando la tabla.
    connect(ui->historyTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_painting || item->column() != DescriptionColumn)
            return;
        const QTableWidgetItem *date = ui->historyTable->item(item->row(), DateColumn);
        if (!date)
            return;
        const int id = date->data(kIdRole).toInt();
        const QString text = item->text();
        QMetaObject::invokeMethod(
            this, [this, id, text] { emit descriptionEdited(id, text); }, Qt::QueuedConnection);
    });
}

MaintenanceHistoryTableWidget::~MaintenanceHistoryTableWidget()
{
    delete ui;
}

QList<QWidget *> MaintenanceHistoryTableWidget::focusOrder() const
{
    return {ui->addMaintenanceButton, ui->historyTable, ui->printReceiptsButton};
}

void MaintenanceHistoryTableWidget::bind(presentation::MaintenanceHistoryPresenter &presenter)
{
    using presentation::MaintenanceHistoryPresenter;
    connect(this, &MaintenanceHistoryTableWidget::addMaintenanceRequested, &presenter,
            &MaintenanceHistoryPresenter::onAddMaintenance);
    connect(this, &MaintenanceHistoryTableWidget::maintenanceSubmitted, &presenter,
            &MaintenanceHistoryPresenter::onSubmitMaintenance);
    connect(this, &MaintenanceHistoryTableWidget::printReceiptsRequested, &presenter,
            &MaintenanceHistoryPresenter::onPrintReceipt);
    connect(this, &MaintenanceHistoryTableWidget::descriptionEdited, &presenter,
            &MaintenanceHistoryPresenter::onDescriptionEdited);
}

void MaintenanceHistoryTableWidget::setBusy(bool busy)
{
    ui->addMaintenanceButton->setEnabled(!busy);
}

void MaintenanceHistoryTableWidget::showMaintenances(
    const QList<application::MaintenanceItemDto> &items)
{
    const QScopedValueRollback<bool> painting(m_painting, true);
    const QLocale locale(QLocale::Spanish, QLocale::Mexico);
    ui->historyTable->setRowCount(0);
    ui->historyTable->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const application::MaintenanceItemDto &item = items.at(row);
        QTableWidgetItem *date = cell(item.date.toString(QStringLiteral("dd/MM/yyyy")));
        date->setData(kIdRole, item.id);
        ui->historyTable->setItem(row, DateColumn, date);
        ui->historyTable->setItem(row, TypeColumn, cell(item.typeName));
        QTableWidgetItem *cost =
            cell(QStringLiteral("$%1").arg(locale.toString(item.cost, 'f', 2)));
        cost->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->historyTable->setItem(row, CostColumn, cost);
        // La única celda que se edita (con doble clic, ver el .ui).
        QTableWidgetItem *description = cell(item.description);
        description->setFlags(description->flags() | Qt::ItemIsEditable);
        ui->historyTable->setItem(row, DescriptionColumn, description);

        // Sin Qt::ItemIsUserCheckable: la casilla se ve, pero el clic no la
        // cambia.
        QTableWidgetItem *receipt = cell(QString());
        receipt->setCheckState(item.receiptPrinted ? Qt::Checked : Qt::Unchecked);
        ui->historyTable->setItem(row, ReceiptColumn, receipt);
    }
}

void MaintenanceHistoryTableWidget::checkReceipt(int maintenanceId)
{
    const QScopedValueRollback<bool> painting(m_painting, true);
    for (int row = 0; row < ui->historyTable->rowCount(); ++row) {
        if (ui->historyTable->item(row, DateColumn)->data(kIdRole).toInt() == maintenanceId)
            ui->historyTable->item(row, ReceiptColumn)->setCheckState(Qt::Checked);
    }
}

void MaintenanceHistoryTableWidget::showWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(m_form ? static_cast<QWidget *>(m_form) : this, title, message);
}

void MaintenanceHistoryTableWidget::showInformation(const QString &message)
{
    QMessageBox::information(this, QStringLiteral("Historial de mantenimientos"), message);
}

bool MaintenanceHistoryTableWidget::confirmDescriptionUpdate()
{
    QMessageBox box(QMessageBox::Question, QStringLiteral("Actualizar descripción"),
                    QStringLiteral("¿Desea actualizar el campo Descripción?"), QMessageBox::NoButton,
                    this);
    // Con estos roles Windows los acomoda abajo a la derecha: Aceptar y, a su
    // derecha, Cancelar.
    QPushButton *accept = box.addButton(QStringLiteral("Aceptar"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    box.setDefaultButton(accept);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == accept;
}

int MaintenanceHistoryTableWidget::selectedMaintenanceId()
{
    const QList<QTableWidgetItem *> selected = ui->historyTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    const QTableWidgetItem *date = ui->historyTable->item(selected.first()->row(), DateColumn);
    return date ? date->data(kIdRole).toInt() : -1;
}

QString MaintenanceHistoryTableWidget::askReceiptPath(const QString &suggestedName)
{
    return QFileDialog::getSaveFileName(this, QStringLiteral("Guardar recibo"), suggestedName,
                                        QStringLiteral("PDF (*.pdf)"));
}

void MaintenanceHistoryTableWidget::openDocument(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MaintenanceHistoryTableWidget::askNewMaintenance(
    const QList<application::CatalogOptionDto> &maintenanceTypes)
{
    if (m_form) {
        m_form->raise();
        m_form->activateWindow();
        return;
    }
    // Hijo de la ventana y no de esta sección: el Paso 1 recorre sus
    // descendientes buscando campos por su clave ("description" también es
    // suya), y no debe encontrar los del formulario.
    m_form = new AddMaintenanceDialog(maintenanceTypes, window());
    m_form->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_form, &AddMaintenanceDialog::submitted, this,
            &MaintenanceHistoryTableWidget::maintenanceSubmitted);
    // open() y no exec(): modal igual, pero sin un bucle de eventos anidado
    // mientras el presenter guarda en otro hilo.
    m_form->open();
}

void MaintenanceHistoryTableWidget::setMaintenanceFormBusy(bool busy)
{
    if (m_form)
        m_form->setBusy(busy);
}

void MaintenanceHistoryTableWidget::showMaintenanceFormErrors(
    const QList<domain::ValidationError> &errors)
{
    if (m_form)
        m_form->showErrors(errors);
}

void MaintenanceHistoryTableWidget::closeMaintenanceForm()
{
    if (m_form)
        m_form->accept();
}
