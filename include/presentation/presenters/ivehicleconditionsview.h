#ifndef PRESENTATION_PRESENTERS_IVEHICLECONDITIONSVIEW_H
#define PRESENTATION_PRESENTERS_IVEHICLECONDITIONSVIEW_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/presenters/istepview.h"

#include <QList>
#include <QString>

namespace presentation {

// Paso 2 del asistente: especificaciones y checklist de condición.
class IVehicleConditionsView : public IStepView
{
public:
    virtual application::VehicleConditionsDto conditions() const = 0;

    virtual void setFuelTypes(const QList<application::CatalogOptionDto> &fuelTypes) = 0;
    virtual void setChecklist(const QList<application::ChecklistItemDto> &items) = 0;
    // Aviso sobre el checklist (por ejemplo, que el catálogo llegó vacío).
    virtual void showChecklistMessage(const QString &message) = 0;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_IVEHICLECONDITIONSVIEW_H
