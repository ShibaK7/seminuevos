#include "presentation/inventory/record/vehiclerecordpresenter.h"

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/inventory/record/ivehiclerecordview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionspresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/files/ivehiclefilesview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"

#include <QStringList>

namespace presentation {

namespace {

constexpr int kStepCount = 3;

// "Folio 12 · Nissan Versa 2019": con qué unidad se está trabajando, sin
// tener que ir a la pestaña de Detalles.
QString recordTitle(int folio, const application::VehicleDetailsDto &details)
{
    QStringList vehicle;
    if (!details.brand.name.isEmpty())
        vehicle << details.brand.name;
    if (!details.model.isEmpty())
        vehicle << details.model;
    if (details.yearModel > 0)
        vehicle << QString::number(details.yearModel);

    const QString title = QStringLiteral("Folio %1").arg(folio);
    if (vehicle.isEmpty())
        return title;
    return title + QStringLiteral(" · ") + vehicle.join(QLatin1Char(' '));
}

} // namespace

VehicleRecordPresenter::VehicleRecordPresenter(IVehicleRecordView &record,
                                               IVehicleDetailsView &details,
                                               IVehicleConditionsView &conditions,
                                               IVehicleFilesView &files,
                                               const application::VehicleRegistrationService &forms,
                                               const application::VehicleRecordService &records,
                                               TaskRunner &runner, int folio, domain::UserRole role,
                                               QObject *parent)
    : QObject(parent)
    , m_record(record)
    , m_forms(forms)
    , m_records(records)
    , m_runner(runner)
    , m_folio(folio)
    , m_role(role)
    , m_details(std::make_unique<VehicleDetailsPresenter>(details, forms))
    , m_conditions(std::make_unique<VehicleConditionsPresenter>(conditions, forms))
    , m_files(std::make_unique<VehicleFilesPresenter>(files, forms))
    , m_nav(kStepCount, true)
{
    m_steps = {m_details.get(), m_conditions.get(), m_files.get()};
}

VehicleRecordPresenter::~VehicleRecordPresenter() = default;

VehicleDetailsPresenter &VehicleRecordPresenter::details()
{
    return *m_details;
}

VehicleFilesPresenter &VehicleRecordPresenter::files()
{
    return *m_files;
}

int VehicleRecordPresenter::folio() const
{
    return m_folio;
}

void VehicleRecordPresenter::start()
{
    // Lo que el rol no deja ver se decide antes de pintar nada, para que no
    // se asome ni un instante.
    m_access = m_records.accessFor(m_role);
    m_record.setDealTermsVisible(m_access.showsDealTerms);
    m_record.setMaintenanceHistoryVisible(m_access.showsMaintenanceHistory);

    // Consulta: las tres páginas en solo lectura desde el principio, así lo
    // que cada una arme después (el checklist llega con los catálogos) nace
    // igual.
    for (WizardStepPresenter *step : m_steps)
        step->setReadOnly(true);
    // Los formatos que admite cada entrada de archivos, para cuando se edite.
    m_files->start();

    m_record.showTitle(QStringLiteral("Folio %1").arg(m_folio));
    showCurrentStep();

    // Mientras llega la primera lectura no hay nada que consultar.
    m_record.setBusy(true);
    load();
}

void VehicleRecordPresenter::refresh()
{
    load();
}

void VehicleRecordPresenter::showMaintenanceCost(double total)
{
    m_details->showMaintenanceCost(total);
    if (m_loaded)
        m_loaded->details.maintenanceCost = total;
}

void VehicleRecordPresenter::load()
{
    // Una sola tarea para catálogos y expediente: los combos se llenan con
    // los catálogos antes de elegir lo guardado, así que el expediente no se
    // puede pintar sin ellos.
    const int generation = ++m_loadGeneration;
    const bool withLookups = !m_lookupsComplete;
    const application::VehicleRegistrationService *forms = &m_forms;
    const application::VehicleRecordService *records = &m_records;
    const int folio = m_folio;
    const domain::UserRole role = m_role;
    m_runner.run(
        this,
        [forms, records, folio, role, withLookups] {
            Loaded loaded;
            if (withLookups)
                loaded.lookups = forms->loadLookups();
            loaded.record = records->load(folio, role);
            return loaded;
        },
        [this, generation](const Loaded &loaded) { onLoaded(generation, loaded); });
}

void VehicleRecordPresenter::onLoaded(int generation, const Loaded &loaded)
{
    // Ya se pidió otra lectura: esta respuesta es vieja.
    if (generation != m_loadGeneration)
        return;
    m_record.setBusy(false);

    QStringList problems;
    if (loaded.lookups) {
        applyLookups(*loaded.lookups);
        if (!loaded.lookups->errorMessage.isEmpty()) {
            problems << QStringLiteral("No se pudieron leer todos los catálogos:\n")
                            + loaded.lookups->errorMessage;
        }
    }

    using Status = application::RecordLoadResult::Status;
    switch (loaded.record.status) {
    case Status::Loaded:
        m_loaded = loaded.record.record;
        break;
    case Status::NotFound:
        m_loaded.reset();
        problems << QStringLiteral("La unidad con folio %1 ya no existe: quizá otro usuario la "
                                   "dio de baja.")
                        .arg(m_folio);
        // El inventario de atrás todavía la muestra.
        emit vehicleChanged(m_folio);
        break;
    case Status::Failed:
        problems << QStringLiteral("No se pudo leer el expediente:\n")
                        + loaded.record.errorMessage;
        break;
    }

    // Aplicar los catálogos vuelve a llenar los combos, así que lo que ya se
    // veía también se repinta, aunque esta lectura haya fallado.
    if (m_loaded && (loaded.record.status == Status::Loaded || loaded.lookups))
        showRecord(*m_loaded);
    m_record.showMessage(problems.join(QStringLiteral("\n\n")));
}

void VehicleRecordPresenter::applyLookups(const application::RegistrationLookupsDto &lookups)
{
    m_details->setLookups(lookups);
    m_conditions->setLookups(lookups);
    // Sin el checklist, guardar borraría la inspección: los catálogos se
    // vuelven a pedir hasta que lleguen completos.
    m_lookupsComplete = lookups.errorMessage.isEmpty() && !lookups.checklist.isEmpty();
}

void VehicleRecordPresenter::showRecord(const application::VehicleRecordDto &record)
{
    m_record.showTitle(recordTitle(record.folio, record.details));
    m_details->showStored(record.folio, record.details);
    m_conditions->showStored(record.conditions);

    QList<ImagePreview> previews;
    for (const application::StoredImageDto &stored : record.images)
        previews << ImagePreview{stored.image.path, stored.thumbnail, stored.image.isPrimary};
    m_files->showStored(previews, record.documents);

    // Solo a quien podría editarla: sin la operación de su rama no hay qué
    // corregir.
    m_record.showNotice(m_access.canEdit && !record.hasDealTerms
                            ? QStringLiteral("Esta unidad no tiene registrados los datos de su "
                                             "compra o consignación: se puede consultar, pero no "
                                             "editar.")
                            : QString());
}

void VehicleRecordPresenter::onStepClicked(int step)
{
    if (step < 0 || step >= kStepCount || step == m_nav.current())
        return;
    m_nav.moveTo(step);
    showCurrentStep();
}

void VehicleRecordPresenter::onBack()
{
    if (confirmLeave())
        emit closeRequested();
}

bool VehicleRecordPresenter::confirmLeave()
{
    // En consulta no hay nada que se pueda perder.
    return true;
}

void VehicleRecordPresenter::showCurrentStep()
{
    m_record.showStep(m_nav.current());
    refreshStepper();
}

void VehicleRecordPresenter::refreshStepper()
{
    // Pestañas, no pasos: la actual y las demás, sin candados ni palomitas.
    QList<StepVisual> states;
    for (int step = 0; step < kStepCount; ++step)
        states << (step == m_nav.current() ? StepVisual::Current : StepVisual::Pending);
    m_record.showStepStates(states);
}

} // namespace presentation
