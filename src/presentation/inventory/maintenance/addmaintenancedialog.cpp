#include "presentation/inventory/maintenance/addmaintenancedialog.h"
#include "ui_addmaintenancedialog.h"

#include "presentation/common/forms/formsupport.h"

#include <QDate>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

AddMaintenanceDialog::AddMaintenanceDialog(
    const QList<application::CatalogOptionDto> &maintenanceTypes, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddMaintenanceDialog)
{
    ui->setupUi(this);
    formsupport::addFieldErrorLabels(this);

    // Hoy depende del día en que se abre: no cabe en el .ui.
    ui->maintenanceDate->setDate(QDate::currentDate());
    formsupport::fillCombo(ui->maintenanceTypeCombo, maintenanceTypes);

    connect(ui->maintenanceAcceptButton, &QPushButton::clicked, this,
            [this] { emit submitted(maintenance()); });
    connect(ui->maintenanceCancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

AddMaintenanceDialog::~AddMaintenanceDialog()
{
    delete ui;
}

application::NewMaintenanceDto AddMaintenanceDialog::maintenance() const
{
    application::NewMaintenanceDto dto;
    dto.date = ui->maintenanceDate->date();
    if (ui->maintenanceTypeCombo->currentIndex() >= 0)
        dto.maintenanceTypeId = ui->maintenanceTypeCombo->currentData().toInt();
    dto.cost = ui->maintenanceCost->text();
    dto.description = ui->maintenanceDescription->toPlainText();
    return dto;
}

void AddMaintenanceDialog::setBusy(bool busy)
{
    ui->maintenanceAcceptButton->setEnabled(!busy);
}

void AddMaintenanceDialog::showErrors(const QList<domain::ValidationError> &errors)
{
    const QList<domain::ValidationError> unshown = formsupport::showFieldErrors(this, errors);
    for (const domain::ValidationError &error : errors) {
        if (formsupport::focusField(this, error.field))
            break;
    }
    if (!unshown.isEmpty()) {
        QStringList messages;
        for (const domain::ValidationError &error : unshown)
            messages << error.message;
        QMessageBox::warning(this, QStringLiteral("No se guardó el mantenimiento"),
                             messages.join(QStringLiteral("\n")));
    }
}
