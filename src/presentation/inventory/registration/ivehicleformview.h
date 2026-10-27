#ifndef PRESENTATION_INVENTORY_REGISTRATION_IVEHICLEFORMVIEW_H
#define PRESENTATION_INVENTORY_REGISTRATION_IVEHICLEFORMVIEW_H

#include <QString>

namespace presentation {

// Lo que comparten los dos marcos del formulario de un vehículo: el asistente
// de alta (IVehicleWizardView) y el expediente (IVehicleRecordView). Cada uno
// agrega lo suyo: el botón primario y el contrato, o el modo y las
// confirmaciones. Las páginas de cada paso tienen su propia interfaz.
class IVehicleFormView
{
public:
    virtual ~IVehicleFormView() = default;

    virtual void showStep(int step) = 0;
    // El aviso bajo las páginas. Vacío = ocultarlo.
    virtual void showMessage(const QString &message) = 0;
    // Ocupado: pasos, páginas y botones deshabilitados.
    virtual void setBusy(bool busy) = 0;

protected:
    IVehicleFormView() = default;
    IVehicleFormView(const IVehicleFormView &) = default;
    IVehicleFormView &operator=(const IVehicleFormView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_REGISTRATION_IVEHICLEFORMVIEW_H
