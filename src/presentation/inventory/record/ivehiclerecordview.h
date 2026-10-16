#ifndef PRESENTATION_INVENTORY_RECORD_IVEHICLERECORDVIEW_H
#define PRESENTATION_INVENTORY_RECORD_IVEHICLERECORDVIEW_H

#include "presentation/common/navigation/wizardnavigator.h"

#include <QList>
#include <QString>

namespace presentation {

// El marco del expediente de un vehículo (US-03.3 y US-03.4): título,
// pestañas, avisos y botones. Las páginas son las mismas del alta, cada una
// con su propia interfaz (IVehicleDetailsView y compañía).
class IVehicleRecordView
{
public:
    virtual ~IVehicleRecordView() = default;

    virtual void showTitle(const QString &title) = 0;
    virtual void showStep(int step) = 0;
    // Cómo se ve cada pestaña. En el expediente nunca hay candados ni
    // palomitas: solo la actual, las demás y, al editar, las que tienen
    // errores.
    virtual void showStepStates(const QList<StepVisual> &states) = 0;
    // El aviso de error bajo las páginas. Vacío = ocultarlo.
    virtual void showMessage(const QString &message) = 0;
    // Una línea neutra, por ejemplo "Cambios guardados.". Vacía = ocultarla.
    virtual void showNotice(const QString &notice) = 0;
    // Ocupado: pestañas, páginas y botones deshabilitados.
    virtual void setBusy(bool busy) = 0;

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
