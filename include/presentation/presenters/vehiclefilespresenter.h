#ifndef PRESENTATION_PRESENTERS_VEHICLEFILESPRESENTER_H
#define PRESENTATION_PRESENTERS_VEHICLEFILESPRESENTER_H

#include "application/dto/registrationdtos.h"
#include "presentation/presenters/wizardsteppresenter.h"

#include <QStringList>

namespace application {
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleFilesView;

// Paso 3: galería y documentos. La vista solo junta rutas; este presenter
// decide cuáles se admiten (por el servicio, que conoce la regla de formatos y
// el disco) y le entrega las miniaturas ya leídas.
class VehicleFilesPresenter final : public WizardStepPresenter
{
public:
    VehicleFilesPresenter(IVehicleFilesView &view,
                          const application::VehicleRegistrationService &service);

    QString title() const override;
    // "images*" y "documents*".
    bool ownsField(const QString &field) const override;

    // Le pasa a la vista los formatos de cada entrada.
    void start();

    // Fotos elegidas en el diálogo o soltadas sobre la galería. Las válidas se
    // agregan aunque otras del mismo lote fallen: rechazar el lote entero por
    // un archivo obligaría a volver a elegir todo.
    void onImagesChosen(const QStringList &paths);
    void onDocumentChosen(const QString &documentType, const QString &path);

    application::VehicleFilesDto dto() const;

protected:
    domain::ValidationResult collectErrors() const override;

private:
    IVehicleFilesView &m_view;
    const application::VehicleRegistrationService &m_service;
};

} // namespace presentation

#endif // PRESENTATION_PRESENTERS_VEHICLEFILESPRESENTER_H
