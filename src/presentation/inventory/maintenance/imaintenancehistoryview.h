#ifndef PRESENTATION_INVENTORY_MAINTENANCE_IMAINTENANCEHISTORYVIEW_H
#define PRESENTATION_INVENTORY_MAINTENANCE_IMAINTENANCEHISTORYVIEW_H

#include "application/common/dto/catalogoptiondto.h"
#include "application/inventory/maintenance/dto/maintenancedtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>

namespace presentation {

// El historial de mantenimientos del expediente, visto desde su presenter. La
// implementa MaintenanceHistoryTableWidget.
class IMaintenanceHistoryView
{
public:
    virtual ~IMaintenanceHistoryView() = default;

    // Mientras se lee lo que necesita el formulario de alta.
    virtual void setBusy(bool busy) = 0;
    // Pinta los renglones de la tabla. La casilla Recibo de cada uno dice si
    // ya se imprimió su recibo; el usuario no la cambia.
    virtual void showMaintenances(const QList<application::MaintenanceItemDto> &items) = 0;
    // Marca la casilla de Recibo del renglón con ese id: ya se imprimió.
    virtual void checkReceipt(int maintenanceId) = 0;
    virtual void showWarning(const QString &title, const QString &message) = 0;
    // Un aviso que no es un error ("El registro se actualizó con éxito").
    virtual void showInformation(const QString &message) = 0;

    // --- Corregir la descripción en la tabla ---
    // "Desea actualizar el campo Descripción", con Aceptar y Cancelar. true
    // si aceptó.
    virtual bool confirmDescriptionUpdate() = 0;

    // --- El recibo ---
    // El id del renglón seleccionado en la tabla, o -1 si no hay.
    virtual int selectedMaintenanceId() = 0;
    // Pide dónde guardar el PDF. Vacío si el usuario canceló.
    virtual QString askReceiptPath(const QString &suggestedName) = 0;
    // Abre un archivo con el programa que el sistema tenga para él.
    virtual void openDocument(const QString &path) = 0;

    // --- El formulario de alta ---
    // Lo abre como modal, con el combo de tipos ya lleno. No espera a que se
    // cierre: lo capturado llega después, con una señal de la vista.
    virtual void askNewMaintenance(const QList<application::CatalogOptionDto> &maintenanceTypes) = 0;
    // Mientras se guarda: Aceptar no se puede volver a presionar.
    virtual void setMaintenanceFormBusy(bool busy) = 0;
    // Marca bajo cada campo lo que reportó el dominio; el formulario sigue
    // abierto.
    virtual void showMaintenanceFormErrors(const QList<domain::ValidationError> &errors) = 0;
    virtual void closeMaintenanceForm() = 0;

protected:
    IMaintenanceHistoryView() = default;
    IMaintenanceHistoryView(const IMaintenanceHistoryView &) = default;
    IMaintenanceHistoryView &operator=(const IMaintenanceHistoryView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_MAINTENANCE_IMAINTENANCEHISTORYVIEW_H
