#ifndef PRESENTATION_PRESENTERS_IVEHICLEDETAILSVIEW_H
#define PRESENTATION_PRESENTERS_IVEHICLEDETAILSVIEW_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/presenters/istepview.h"

#include <QString>

namespace presentation {

// Cómo se ve la fila de la factura. Lo decide InvoiceAttachment; la vista
// solo lo pinta.
struct InvoiceAttachmentState
{
    QString fileLabel;     // "Sin archivo", el nombre del archivo o "Factura retirada"
    QString fileToolTip;   // por qué se retiró, si se retiró
    bool cfdiButtonVisible = false;
    bool uploadEnabled = true;
    QString uploadToolTip; // por qué está bloqueada la subida
};

// Paso 1 del asistente: datos del vehículo, contraparte y operación.
class IVehicleDetailsView : public IStepView
{
public:
    // Lo capturado, tal cual. El archivo de factura NO viene aquí: lo lleva
    // el presenter (InvoiceAttachment), que es quien sabe si cuenta.
    virtual application::VehicleDetailsDto details() const = 0;

    // Llena Tipo Vehículo, Subtipo y Marca.
    virtual void setLookups(const application::RegistrationLookupsDto &lookups) = 0;

    virtual void showInvoiceAttachment(const InvoiceAttachmentState &state) = 0;

    // Pide al usuario el archivo de la factura. Vacío si canceló.
    virtual QString askInvoiceFile() = 0;
    // Abre un archivo con el programa que el sistema tenga para él.
    virtual bool openDocument(const QString &path) = 0;
    virtual void showWarning(const QString &title, const QString &message) = 0;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_IVEHICLEDETAILSVIEW_H
