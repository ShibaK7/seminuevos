#ifndef PRESENTATION_INVENTORY_RECORD_IVEHICLERECORDVIEW_H
#define PRESENTATION_INVENTORY_RECORD_IVEHICLERECORDVIEW_H

#include "presentation/common/navigation/editsession.h"
#include "presentation/common/navigation/wizardnavigator.h"
#include "presentation/inventory/registration/ivehicleformview.h"

#include <QList>
#include <QString>

namespace presentation {

// El marco del expediente de un vehículo (US-03.3 y US-03.4): título,
// pestañas, avisos y botones. Las páginas son las mismas del alta, cada una
// con su propia interfaz (IVehicleDetailsView y compañía). Lo que comparte
// con el asistente (la pestaña que se ve, el aviso de error, ocupado) viene
// de IVehicleFormView.
class IVehicleRecordView : public IVehicleFormView
{
public:
    virtual void showTitle(const QString &title) = 0;
    // Cómo se ve cada pestaña. En el expediente nunca hay candados ni
    // palomitas: solo la actual, las demás y, al editar, las que tienen
    // errores.
    virtual void showStepStates(const QList<StepVisual> &states) = 0;
    // Una línea neutra, por ejemplo "Cambios guardados.". Vacía = ocultarla.
    virtual void showNotice(const QString &notice) = 0;

    // Los botones de cada modo (#33). En consulta: Volver y, si el rol puede
    // editar, Editar, habilitado solo si ahora se puede (expediente cargado y
    // sin una lectura en curso). En edición: Cancelar y Guardar cambios.
    virtual void showMode(FormMode mode, bool canEdit, bool editEnabled) = 0;
    // Si se descartan los cambios sin guardar.
    virtual bool confirmDiscardChanges() = 0;
    // Si se guarda aunque eso quite del expediente (y borre del almacén) la
    // factura que tenía.
    virtual bool confirmInvoiceRemoval(const QString &fileName) = 0;

    // Lo que el rol de quien entró deja ver (lo decide VehicleRecordAccess).
    // La tarjeta de la operación: contraparte, factura, precios.
    virtual void setDealTermsVisible(bool visible) = 0;
    virtual void setMaintenanceHistoryVisible(bool visible) = 0;

protected:
    IVehicleRecordView() = default;
    IVehicleRecordView(const IVehicleRecordView &) = default;
    IVehicleRecordView &operator=(const IVehicleRecordView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_RECORD_IVEHICLERECORDVIEW_H
