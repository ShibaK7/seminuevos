#ifndef PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_IVEHICLEDETAILSVIEW_H
#define PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_IVEHICLEDETAILSVIEW_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/inventory/acquisition/iacquisitiontermsview.h"
#include "presentation/inventory/registration/istepview.h"

#include <QString>

namespace presentation {

// Paso 1 del asistente: datos del vehículo, contraparte y operación. Lo que
// cambia entre ramas vive en una sección aparte de cada una (la de Adquisición
// tiene su propia interfaz porque el presenter le habla).
class IVehicleDetailsView : public IStepView
{
public:
    // Lo capturado, tal cual, de las dos ramas. El archivo de factura NO viene
    // aquí: lo lleva el presenter de Adquisición, que es quien sabe si cuenta.
    virtual application::VehicleDetailsDto details() const = 0;

    // Llena Tipo Vehículo, Subtipo y Marca.
    virtual void setLookups(const application::RegistrationLookupsDto &lookups) = 0;

    // Pinta una unidad ya guardada (el expediente): su folio y lo capturado,
    // de modo que details() devuelva lo mismo. Va después de setLookups().
    // La factura adjunta tampoco viene aquí: su fila la pinta el presenter de
    // Adquisición.
    virtual void showDetails(int folio, const application::VehicleDetailsDto &details) = 0;
    // Solo el total de mantenimientos, que crece con el historial.
    virtual void showMaintenanceCost(double cost) = 0;

    // La sección de Adquisición (archivo de factura, precios y pago).
    virtual IAcquisitionTermsView &acquisitionTerms() = 0;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_IVEHICLEDETAILSVIEW_H
