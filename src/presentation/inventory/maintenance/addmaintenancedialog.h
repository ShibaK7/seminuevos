#ifndef PRESENTATION_INVENTORY_MAINTENANCE_ADDMAINTENANCEDIALOG_H
#define PRESENTATION_INVENTORY_MAINTENANCE_ADDMAINTENANCEDIALOG_H

#include "application/common/dto/catalogoptiondto.h"
#include "application/inventory/maintenance/dto/maintenancedtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDialog>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui {
    class AddMaintenanceDialog;
}
QT_END_NAMESPACE

// El formulario modal de "+ Agregar Mantenimiento": fecha (hoy de entrada),
// tipo (del catálogo vehicle_maintenance_cat), costo y descripción. Su
// formulario es addmaintenancedialog.ui, en esta misma carpeta.
//
// Vista pasiva: Aceptar no cierra ni valida nada, solo entrega lo capturado
// (submitted). Quien guarda decide si cierra (accept()) o marca los errores
// que reportó el dominio (showErrors()). Cancelar sí lo cierra solo.
class AddMaintenanceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddMaintenanceDialog(const QList<application::CatalogOptionDto> &maintenanceTypes,
                                  QWidget *parent = nullptr);
    ~AddMaintenanceDialog() override;

    // Lo capturado, sin folio (lo pone quien guarda).
    application::NewMaintenanceDto maintenance() const;

    // Mientras se guarda, Aceptar no se puede volver a presionar.
    void setBusy(bool busy);
    // Marca cada campo con su error y le da el foco al primero. Un error sin
    // campo en el formulario se avisa aparte.
    void showErrors(const QList<domain::ValidationError> &errors);

signals:
    // Aceptar.
    void submitted(const application::NewMaintenanceDto &maintenance);

private:
    Ui::AddMaintenanceDialog *ui;
};

#endif // PRESENTATION_INVENTORY_MAINTENANCE_ADDMAINTENANCEDIALOG_H
