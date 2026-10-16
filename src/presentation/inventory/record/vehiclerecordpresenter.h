#ifndef PRESENTATION_INVENTORY_RECORD_VEHICLERECORDPRESENTER_H
#define PRESENTATION_INVENTORY_RECORD_VEHICLERECORDPRESENTER_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/recorddtos.h"
#include "domain/common/value_objects/enums.h"
#include "presentation/common/navigation/editsession.h"
#include "presentation/common/navigation/wizardnavigator.h"

#include <QObject>
#include <QSet>
#include <QTimer>

#include <array>
#include <memory>
#include <optional>

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
// Dos modos (EditSession):
//   - Consulta: las tres páginas en solo lectura, sin validar nada.
//   - Edición (solo quien puede editar): los campos se desbloquean, salvo la
//     rama y el total de mantenimientos, y Folio, que nunca se edita. Cada
//     edición marca el formulario como modificado; Guardar valida los tres
//     pasos y lleva los errores a su campo, y Cancelar vuelve a lo guardado.
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

    // Aplica lo que el rol deja ver, muestra la primera pestaña y lee los
    // catálogos y el expediente (fuera del hilo de la interfaz).
    void start();
    // Vuelve a leer el expediente. Es el enganche para quien cambia la unidad
    // por fuera, como el historial de mantenimientos. Una lectura anterior
    // que todavía no llega se descarta; una que llega a media edición no pisa
    // lo capturado: se pinta al volver a consulta.
    void refresh();
    // Repinta solo el total de mantenimientos, que crece con el historial: es
    // barato y no vuelve a leer nada. También a media edición: el campo está
    // bloqueado y no cuenta como un cambio.
    void showMaintenanceCost(double total);

    // Las pestañas son libres: se entra a cualquiera sin validar nada.
    void onStepClicked(int step);
    // "Volver al inventario".
    void onBack();
    // Si se puede salir del expediente (cerrarlo, reemplazarlo por otro o
    // cerrar la ventana): pregunta solo si hay cambios sin guardar, y mientras
    // se guarda pide esperar.
    bool confirmLeave();

    // --- Edición (US-03.4) ---
    void onEdit();
    void onSave();
    void onCancel();
    // Algo cambió en el paso `step`. Solo cuenta al editar, y fuera de lo que
    // el presenter pinta: marca el formulario como modificado y revalida con
    // un debounce.
    void onStepEdited(int step);
    // Los botones de la factura: solo cuentan como cambio si cambió la
    // factura adjunta (cancelar el diálogo no lo es).
    void onBrowseInvoice();
    void onCfdiRequest();
    // Corre ya la revalidación pendiente, sin esperar el debounce.
    void revalidateEditedSteps();

signals:
    // La unidad cambió o ya no existe: el inventario de atrás debe releerla.
    void vehicleChanged(int folio);
    // Pide cerrar el expediente y volver al inventario.
    void closeRequested();

private:
    // Lo que trae una lectura: los catálogos (solo mientras no hayan llegado
    // completos) y el expediente.
    struct Loaded
    {
        std::optional<application::RegistrationLookupsDto> lookups;
        application::RecordLoadResult record;
    };

    void load();
    void onLoaded(int generation, const Loaded &loaded);
    void onSaved(const application::RecordUpdateResult &result);
    void applyLookups(const application::RegistrationLookupsDto &lookups);
    void showRecord(const application::VehicleRecordDto &record);
    // De vuelta a consulta: sin errores marcados ni intentos, en solo
    // lectura y sin cambios pendientes.
    void enterView();
    // Si Editar se puede usar ahora, y los botones del modo actual.
    bool editAvailable() const;
    void refreshMode();
    // El aviso neutro: el último de una acción ("Cambios guardados.") o, si
    // no hay, por qué la unidad no se puede editar.
    void showNotice(const QString &notice);
    void refreshNotice();
    // Si guardar quitaría la factura que tenía la unidad (por ejemplo, al
    // pasar a Autofactura sin una solicitud nueva, que la aparta).
    bool invoiceWouldBeRemoved(QString *fileName) const;
    int stepOwning(const domain::ValidationResult &result) const;
    void showInvalidStep(int step, const domain::ValidationResult &result);
    domain::ValidationResult markStepFields(int step, const domain::ValidationResult &result);
    void showValidationErrors(const QString &title, const domain::ValidationResult &unshown);
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

    // Navegación libre: las pestañas no se bloquean ni llevan palomita. Al
    // guardar recuerda qué pasos se intentaron, para marcar sus errores.
    WizardNavigator m_nav;
    EditSession m_session;

    // Debounce de la revalidación en vivo, como en el alta.
    QTimer m_revalidateTimer;
    QSet<int> m_editedSteps;

    // Cada lectura lleva un número; solo se pinta la respuesta de la última.
    // Así una respuesta vieja o que llega fuera de orden no pisa a una nueva.
    int m_loadGeneration = 0;
    bool m_loading = false;
    bool m_saving = false;
    // Mientras el presenter pinta, los campos avisan cambios que no son del
    // usuario.
    bool m_painting = false;
    // Se guardó y todavía no llega la relectura: lo que hay en memoria tiene
    // la versión vieja, así que no se puede volver a editar.
    bool m_rereadPending = false;
    // Los catálogos se piden hasta que llegan completos, y se aplican antes
    // de pintar el expediente (los combos se llenan con ellos).
    bool m_lookupsComplete = false;
    // El último expediente leído, tal como se pintó; Cancelar vuelve a él.
    std::optional<application::VehicleRecordDto> m_loaded;
    // Una lectura que llegó a media edición: se pinta al volver a consulta.
    std::optional<application::VehicleRecordDto> m_pendingReload;
    QString m_notice;
    // Por qué se volvió a leer (otro usuario la cambió): el aviso se queda
    // también cuando llega la lectura.
    QString m_reloadReason;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_RECORD_VEHICLERECORDPRESENTER_H
