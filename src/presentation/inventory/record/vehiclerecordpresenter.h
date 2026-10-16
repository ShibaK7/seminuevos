#ifndef PRESENTATION_INVENTORY_RECORD_VEHICLERECORDPRESENTER_H
#define PRESENTATION_INVENTORY_RECORD_VEHICLERECORDPRESENTER_H

#include "application/inventory/registration/dto/recorddtos.h"
#include "domain/common/value_objects/enums.h"
#include "presentation/common/navigation/wizardnavigator.h"

#include <QObject>

#include <array>
#include <memory>

namespace application {
class VehicleRecordService;
class VehicleRegistrationService;
}

namespace presentation {

class IVehicleConditionsView;
class IVehicleDetailsView;
class IVehicleFilesView;
class IVehicleRecordView;
class TaskRunner;
class VehicleConditionsPresenter;
class VehicleDetailsPresenter;
class VehicleFilesPresenter;
class WizardStepPresenter;

// Presenter del expediente de un vehículo (US-03.3 y US-03.4): la misma
// captura del alta, abierta para un folio que ya existe. Usa las mismas tres
// páginas y los mismos presenters de paso que el asistente, así que la
// validación, la regla de la autofactura y los formatos de archivo son los
// mismos; lo que cambia es el marco (pestañas libres, Volver y Editar) y que
// los datos vienen de la base.
//
// Sin widgets (solo QtCore): se prueba con vistas falsas.
class VehicleRecordPresenter final : public QObject
{
    Q_OBJECT

public:
    VehicleRecordPresenter(IVehicleRecordView &record, IVehicleDetailsView &details,
                           IVehicleConditionsView &conditions, IVehicleFilesView &files,
                           const application::VehicleRegistrationService &forms,
                           const application::VehicleRecordService &records, TaskRunner &runner,
                           int folio, domain::UserRole role, QObject *parent = nullptr);
    ~VehicleRecordPresenter() override;

    VehicleDetailsPresenter &details();
    VehicleFilesPresenter &files();
    int folio() const;

    // Aplica lo que el rol deja ver y muestra la primera pestaña.
    void start();
    // Vuelve a leer el expediente. Es el enganche para quien cambia la unidad
    // por fuera, como el historial de mantenimientos.
    void refresh();

    // Las pestañas son libres: se entra a cualquiera sin validar nada.
    void onStepClicked(int step);
    // "Volver al inventario".
    void onBack();
    // Si se puede salir del expediente (cerrarlo o reemplazarlo por otro).
    bool confirmLeave();

signals:
    // La unidad cambió o ya no existe: el inventario de atrás debe releerla.
    void vehicleChanged(int folio);
    // Pide cerrar el expediente y volver al inventario.
    void closeRequested();

private:
    void showCurrentStep();
    void refreshStepper();

    IVehicleRecordView &m_record;
    const application::VehicleRegistrationService &m_forms;
    const application::VehicleRecordService &m_records;
    TaskRunner &m_runner;
    const int m_folio;
    const domain::UserRole m_role;
    // Lo que el rol deja ver y hacer, ya decidido por el dominio.
    application::RecordAccessDto m_access;

    std::unique_ptr<VehicleDetailsPresenter> m_details;
    std::unique_ptr<VehicleConditionsPresenter> m_conditions;
    std::unique_ptr<VehicleFilesPresenter> m_files;
    // Los tres en el orden de las pestañas.
    std::array<WizardStepPresenter *, 3> m_steps{};

    // Navegación libre: las pestañas no se bloquean ni llevan palomita.
    WizardNavigator m_nav;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_RECORD_VEHICLERECORDPRESENTER_H
