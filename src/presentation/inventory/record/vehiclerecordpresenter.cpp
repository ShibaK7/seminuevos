#include "presentation/inventory/record/vehiclerecordpresenter.h"

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "presentation/common/tasks/taskrunner.h"
#include "presentation/inventory/record/ivehiclerecordview.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionspresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/files/ivehiclefilesview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"

#include <QLatin1Char>
#include <QScopedValueRollback>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace presentation {

namespace {

constexpr int kStepCount = 3;

// Cuánto se espera después de la última edición para revalidar, como en el
// alta.
constexpr int kRevalidateDelayMs = 150;

// Lo que no se corrige al editar el expediente: la rama decide qué subtabla
// guarda la operación, y el total de mantenimientos lo acumula el historial.
// Folio no hace falta: ya viene deshabilitado del .ui y nunca se habilita.
QStringList lockedWhileEditing()
{
    return {QStringLiteral("acquisitionType"), QStringLiteral("maintenanceCost")};
}

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

// El nombre del archivo sin su carpeta, sin preguntarle nada al disco.
QString fileNameOf(const QString &path)
{
    const qsizetype separator =
        std::max(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    return path.mid(separator + 1);
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

    m_revalidateTimer.setSingleShot(true);
    m_revalidateTimer.setInterval(kRevalidateDelayMs);
    connect(&m_revalidateTimer, &QTimer::timeout, this,
            &VehicleRecordPresenter::revalidateEditedSteps);
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
    {
        const QScopedValueRollback<bool> painting(m_painting, true);
        m_details->showMaintenanceCost(total);
    }
    if (m_loaded)
        m_loaded->details.maintenanceCost = total;
    if (m_pendingReload)
        m_pendingReload->details.maintenanceCost = total;
}

void VehicleRecordPresenter::load()
{
    // Una sola tarea para catálogos y expediente: los combos se llenan con
    // los catálogos antes de elegir lo guardado, así que el expediente no se
    // puede pintar sin ellos.
    const int generation = ++m_loadGeneration;
    m_loading = true;
    refreshMode();

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
    m_loading = false;
    m_record.setBusy(false);

    using Status = application::RecordLoadResult::Status;

    // Lo que el usuario está capturando no se pisa: lo leído se pinta al
    // volver a consulta. Los catálogos no pueden llegar aquí, porque sin ellos
    // completos no se entra a editar.
    if (m_session.isEditing()) {
        if (loaded.record.status == Status::Loaded)
            m_pendingReload = loaded.record.record;
        refreshMode();
        return;
    }

    QStringList problems;
    if (!m_reloadReason.isEmpty())
        problems << std::exchange(m_reloadReason, QString());
    if (loaded.lookups) {
        applyLookups(*loaded.lookups);
        if (!loaded.lookups->errorMessage.isEmpty()) {
            problems << QStringLiteral("No se pudieron leer todos los catálogos:\n")
                            + loaded.lookups->errorMessage;
        }
    }

    switch (loaded.record.status) {
    case Status::Loaded:
        m_loaded = loaded.record.record;
        m_pendingReload.reset();
        m_rereadPending = false;
        break;
    case Status::NotFound:
        m_loaded.reset();
        m_pendingReload.reset();
        problems << QStringLiteral("La unidad con folio %1 ya no existe: quizá otro usuario la "
                                   "dio de baja.")
                        .arg(m_folio);
        // El inventario de atrás todavía la muestra.
        emit vehicleChanged(m_folio);
        break;
    case Status::Failed:
        problems << (m_rereadPending
                         ? QStringLiteral("Los cambios se guardaron, pero no se pudo releer el "
                                          "expediente; vuelve a abrirlo:\n")
                         : QStringLiteral("No se pudo leer el expediente:\n"))
                        + loaded.record.errorMessage;
        break;
    }

    // Aplicar los catálogos vuelve a llenar los combos, así que lo que ya se
    // veía también se repinta, aunque esta lectura haya fallado.
    if (m_loaded && (loaded.record.status == Status::Loaded || loaded.lookups))
        showRecord(*m_loaded);
    m_record.showMessage(problems.join(QStringLiteral("\n\n")));
    refreshMode();
}

void VehicleRecordPresenter::applyLookups(const application::RegistrationLookupsDto &lookups)
{
    const QScopedValueRollback<bool> painting(m_painting, true);
    m_details->setLookups(lookups);
    m_conditions->setLookups(lookups);
    // Sin el checklist, guardar borraría la inspección: los catálogos se
    // vuelven a pedir hasta que lleguen completos, y mientras no se edita.
    m_lookupsComplete = lookups.errorMessage.isEmpty() && !lookups.checklist.isEmpty();
}

void VehicleRecordPresenter::showRecord(const application::VehicleRecordDto &record)
{
    // Pintar dispara las señales de edición de los campos: no son cambios del
    // usuario.
    const QScopedValueRollback<bool> painting(m_painting, true);

    m_record.showTitle(recordTitle(record.folio, record.details));
    m_details->showStored(record.folio, record.details);
    m_conditions->showStored(record.conditions);

    QList<ImagePreview> previews;
    for (const application::StoredImageDto &stored : record.images)
        previews << ImagePreview{stored.image.path, stored.thumbnail, stored.image.isPrimary};
    m_files->showStored(previews, record.documents);

    refreshNotice();
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
    // Lo que se está guardando no se interrumpe: hay que esperar a saber si
    // quedó.
    if (m_saving) {
        m_record.showMessage(QStringLiteral("Espera a que terminen de guardarse los cambios."));
        return false;
    }
    if (!m_session.needsConfirmationToLeave())
        return true;
    if (!m_record.confirmDiscardChanges())
        return false;
    // Descartados: lo que quede en pantalla ya no es un cambio pendiente.
    enterView();
    return true;
}

void VehicleRecordPresenter::onEdit()
{
    m_session.allowEditing(editAvailable());
    if (!m_session.beginEdit())
        return;

    // Lo que no se corrige queda gris; lo demás, editable.
    for (WizardStepPresenter *step : m_steps)
        step->setReadOnly(false, lockedWhileEditing());
    m_nav.resetAttempts();
    m_notice.clear();
    refreshNotice();
    m_record.showMessage(QString());
    refreshStepper();
    refreshMode();
}

void VehicleRecordPresenter::onSave()
{
    if (!m_session.isEditing() || m_saving || !m_loaded)
        return;
    // Lo pendiente del debounce sobra: aquí se valida todo.
    m_revalidateTimer.stop();
    m_editedSteps.clear();

    // Sin cambios no se valida: las reglas de hoy (la UMA, el año) no deben
    // marcar errores en datos que nadie tocó.
    if (!m_session.isDirty()) {
        enterView();
        showNotice(QStringLiteral("No hubo cambios que guardar."));
        return;
    }

    // Los tres pasos, no solo el que se ve: cada uno marca sus campos, y se
    // lleva al usuario al primero que falle.
    m_nav.markAllAttempted();
    int firstInvalidStep = -1;
    domain::ValidationResult firstInvalidResult;
    for (int step = 0; step < kStepCount; ++step) {
        domain::ValidationResult result = m_steps[step]->validate();
        m_nav.setValid(step, result.isValid());
        markStepFields(step, result);
        if (!result.isValid() && firstInvalidStep < 0) {
            firstInvalidStep = step;
            firstInvalidResult = std::move(result);
        }
    }
    if (firstInvalidStep >= 0) {
        showInvalidStep(firstInvalidStep, firstInvalidResult);
        return;
    }
    refreshStepper();

    // Quitar la factura la borra del almacén al guardar: se pregunta antes.
    QString invoiceName;
    if (invoiceWouldBeRemoved(&invoiceName) && !m_record.confirmInvoiceRemoval(invoiceName))
        return;

    application::VehicleUpdateDto update;
    update.folio = m_folio;
    update.version = m_loaded->version;
    update.details = m_details->dto();
    update.conditions = m_conditions->dto();
    update.files = m_files->dto();

    m_saving = true;
    m_record.setBusy(true);
    m_record.showMessage(QString());
    refreshMode();
    const application::VehicleRecordService *records = &m_records;
    const domain::UserRole role = m_role;
    m_runner.run(
        this, [records, update, role] { return records->update(update, role); },
        [this](const application::RecordUpdateResult &result) { onSaved(result); });
}

void VehicleRecordPresenter::onSaved(const application::RecordUpdateResult &result)
{
    m_saving = false;
    m_record.setBusy(false);

    using Status = application::RecordUpdateResult::Status;
    switch (result.status) {
    case Status::Saved:
        // El inventario de atrás se relee, y el expediente también: se ve como
        // quedó en la base.
        emit vehicleChanged(m_folio);
        m_rereadPending = true;
        enterView();
        showNotice(QStringLiteral("Cambios guardados."));
        load();
        return;
    case Status::Rejected: {
        // Las reglas se revisaron otra vez al guardar, ya contra la base (un
        // VIN o una identificación que son de otro): cada error va a su paso.
        const int step = stepOwning(result.validation);
        m_nav.setValid(step, false);
        markStepFields(step, result.validation);
        showInvalidStep(step, result.validation);
        refreshMode();
        return;
    }
    case Status::Conflict:
        // Lo capturado ya no se puede poner encima: se lee lo del otro usuario.
        enterView();
        m_pendingReload.reset();
        m_reloadReason = QStringLiteral("Otro usuario modificó esta unidad mientras la editabas: "
                                        "tus cambios no se guardaron y se cargaron los suyos.");
        m_record.showMessage(m_reloadReason);
        load();
        return;
    case Status::NotFound:
        enterView();
        m_loaded.reset();
        m_pendingReload.reset();
        m_record.showMessage(QStringLiteral("La unidad con folio %1 ya no existe: quizá otro "
                                            "usuario la dio de baja.")
                                 .arg(m_folio));
        emit vehicleChanged(m_folio);
        refreshMode();
        return;
    case Status::Failed:
        // Disco o base: se queda en edición, con lo capturado, para reintentar.
        m_record.showMessage(QStringLiteral("No se pudieron guardar los cambios:\n")
                             + result.errorMessage);
        refreshMode();
        return;
    }
}

void VehicleRecordPresenter::onCancel()
{
    if (!m_session.isEditing() || m_saving)
        return;
    if (m_session.isDirty() && !m_record.confirmDiscardChanges())
        return;

    enterView();
    // Lo guardado otra vez en pantalla, desde memoria: no hace falta ir a la
    // base. Si a media edición llegó una lectura más nueva, es esa.
    if (m_pendingReload) {
        m_loaded = std::move(m_pendingReload);
        m_pendingReload.reset();
    }
    if (m_loaded)
        showRecord(*m_loaded);
}

void VehicleRecordPresenter::enterView()
{
    m_revalidateTimer.stop();
    m_editedSteps.clear();
    for (WizardStepPresenter *step : m_steps) {
        step->showErrors(domain::ValidationResult());
        step->setReadOnly(true);
    }
    m_nav.resetAttempts();
    m_session.endEdit();
    m_record.showMessage(QString());
    refreshStepper();
    refreshMode();
}

void VehicleRecordPresenter::onStepEdited(int step)
{
    if (m_painting || !m_session.isEditing() || step < 0 || step >= kStepCount)
        return;
    m_session.markEdited();
    m_editedSteps.insert(step);
    // start() reinicia la cuenta si ya corría: la revalidación espera a que
    // el usuario deje de teclear.
    m_revalidateTimer.start();
}

void VehicleRecordPresenter::onBrowseInvoice()
{
    if (m_details->acquisitionTerms().onBrowseInvoice())
        onStepEdited(0);
}

void VehicleRecordPresenter::onCfdiRequest()
{
    if (m_details->acquisitionTerms().onCfdiRequest())
        onStepEdited(0);
}

void VehicleRecordPresenter::revalidateEditedSteps()
{
    m_revalidateTimer.stop();
    const QSet<int> editedSteps = std::exchange(m_editedSteps, QSet<int>());
    for (int step : editedSteps) {
        const domain::ValidationResult result = m_steps[step]->validate();
        m_nav.setValid(step, result.isValid());

        // Los errores se muestran solo en un paso que ya se intentó guardar:
        // antes de eso, nada en rojo mientras se corrige.
        if (!m_nav.isAttempted(step))
            continue;

        // Los campos siguen a la corrección, pero el foco se queda donde está.
        const domain::ValidationResult unshown = markStepFields(step, result);
        if (step == m_nav.current()) {
            if (result.isValid())
                m_record.showMessage(QString());
            else
                showValidationErrors(QStringLiteral("Revisa los campos marcados"), unshown);
        }
    }
    refreshStepper();
}

bool VehicleRecordPresenter::editAvailable() const
{
    // Con el expediente leído tal como está en la base: con sus catálogos
    // completos (sin el checklist, guardar borraría la inspección), con la
    // operación de su rama, sin una lectura o un guardado en curso y sin una
    // relectura pendiente (lo que hay en memoria tendría la versión vieja).
    return m_access.canEdit && m_loaded && m_loaded->hasDealTerms && m_lookupsComplete
           && !m_loading && !m_saving && !m_rereadPending;
}

void VehicleRecordPresenter::refreshMode()
{
    m_record.showMode(m_session.mode(), m_access.canEdit, editAvailable());
}

void VehicleRecordPresenter::showNotice(const QString &notice)
{
    m_notice = notice;
    refreshNotice();
}

void VehicleRecordPresenter::refreshNotice()
{
    if (!m_notice.isEmpty()) {
        m_record.showNotice(m_notice);
        return;
    }
    // Solo a quien podría editarla: sin la operación de su rama no hay qué
    // corregir.
    const bool withoutDeal = m_access.canEdit && m_loaded && !m_loaded->hasDealTerms;
    m_record.showNotice(withoutDeal ? QStringLiteral("Esta unidad no tiene registrados los datos "
                                                     "de su compra o consignación: se puede "
                                                     "consultar, pero no editar.")
                                    : QString());
}

bool VehicleRecordPresenter::invoiceWouldBeRemoved(QString *fileName) const
{
    const QString stored = m_loaded ? m_loaded->details.invoiceFilePath : QString();
    if (stored.isEmpty() || !m_details->dto().invoiceFilePath.isEmpty())
        return false;
    *fileName = fileNameOf(stored);
    return true;
}

int VehicleRecordPresenter::stepOwning(const domain::ValidationResult &result) const
{
    // El primer paso que tenga errores, en el orden de las pestañas.
    for (int step = 0; step < kStepCount; ++step) {
        for (const domain::ValidationError &error : result.errors()) {
            if (m_steps[step]->ownsField(error.field))
                return step;
        }
    }
    return 0;
}

void VehicleRecordPresenter::showInvalidStep(int step, const domain::ValidationResult &result)
{
    m_nav.moveTo(step);
    showCurrentStep();
    // Quien llama ya marcó los campos; marcarlos otra vez dice qué errores no
    // quedaron a la vista, que son los que lleva el aviso.
    showValidationErrors(QStringLiteral("Revisa los campos marcados"),
                         markStepFields(step, result));
    // El foco se mueve solo aquí: el usuario acaba de pedir guardar.
    m_steps[step]->focusFirstError(result);
}

domain::ValidationResult VehicleRecordPresenter::markStepFields(int step,
                                                                const domain::ValidationResult &result)
{
    return m_steps[step]->showErrors(result);
}

void VehicleRecordPresenter::showValidationErrors(const QString &title,
                                                  const domain::ValidationResult &unshown)
{
    // Cada mensaje ya se ve bajo su campo; el aviso lista solo los que no
    // tienen campo en pantalla.
    if (unshown.isValid()) {
        m_record.showMessage(title);
        return;
    }
    m_record.showMessage(title + QStringLiteral(":\n• ")
                         + unshown.joinedMessages(QStringLiteral("\n• ")));
}

void VehicleRecordPresenter::showCurrentStep()
{
    m_record.showStep(m_nav.current());
    refreshStepper();
}

void VehicleRecordPresenter::refreshStepper()
{
    // Pestañas, no pasos: la actual, las demás y, al guardar, las que tienen
    // errores. Nunca candados ni palomitas.
    QList<StepVisual> states;
    for (int step = 0; step < kStepCount; ++step) {
        if (step == m_nav.current())
            states << StepVisual::Current;
        else if (m_nav.isAttempted(step) && !m_nav.isValid(step))
            states << StepVisual::Error;
        else
            states << StepVisual::Pending;
    }
    m_record.showStepStates(states);
}

} // namespace presentation
