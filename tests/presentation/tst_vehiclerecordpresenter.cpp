// Pruebas del presenter del expediente de un vehículo (US-03.3 y US-03.4) con
// vistas falsas y los servicios reales sobre puertos falsos: qué deja ver
// cada rol, cómo se lee y se pinta lo guardado y cómo se navega, sin
// ventanas, sin base y sin disco.

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "presentation/inventory/record/vehiclerecordpresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "registrationfixtures.h"
#include "wizardfakes.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using presentation::StepVisual;

class TstVehicleRecordPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryVehicleRepository m_vehicles;
    fakes::FakeFileStorage m_files;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeContractGenerator m_contracts;
    fakes::FakeVehicleRecordReader m_recordReader;
    fakes::InlineTaskRunner m_runner;

    fakes::FakeRecordView m_record;
    fakes::FakeDetailsView m_details;
    fakes::FakeConditionsView m_conditions;
    fakes::FakeFilesView m_filesView;

    std::unique_ptr<application::VehicleRegistrationService> m_forms;
    std::unique_ptr<application::VehicleRecordService> m_records;
    std::unique_ptr<presentation::VehicleRecordPresenter> m_presenter;

    void createServices()
    {
        m_forms = std::make_unique<application::VehicleRegistrationService>(
            m_vehicles, m_files, m_reference, m_contracts);
        m_records = std::make_unique<application::VehicleRecordService>(m_recordReader, m_vehicles,
                                                                       m_files, m_reference);
    }

    void startPresenter(domain::UserRole role = domain::UserRole::Admin, int folio = 7)
    {
        startWith(m_runner, role, folio);
    }

    void startWith(presentation::TaskRunner &runner, domain::UserRole role = domain::UserRole::Admin,
                   int folio = 7)
    {
        createServices();
        m_presenter = std::make_unique<presentation::VehicleRecordPresenter>(
            m_record, m_details, m_conditions, m_filesView, *m_forms, *m_records, runner, folio,
            role);
        m_presenter->start();
    }

    // Entra a editar y cambia algo del Paso 1, como lo haría la vista.
    void editModel(const QString &model)
    {
        m_presenter->onEdit();
        m_details.captured.model = model;
        m_presenter->onStepEdited(0);
    }

private slots:
    void init()
    {
        m_presenter.reset();
        m_records.reset();
        m_forms.reset();
        m_vehicles = fakes::InMemoryVehicleRepository();
        m_files = fakes::FakeFileStorage();
        m_reference = fakes::FakeReferenceDataReader();
        m_contracts = fakes::FakeContractGenerator();
        m_recordReader = fakes::FakeVehicleRecordReader();
        m_recordReader.records.insert(
            7, fixtures::storedRecord(7, domain::AcquisitionType::Adquisicion));
        m_runner.submitted = 0;
        m_record = fakes::FakeRecordView();
        m_details = fakes::FakeDetailsView();
        m_conditions = fakes::FakeConditionsView();
        m_filesView = fakes::FakeFilesView();
    }

    void opensOnDetailsWithTheFolio()
    {
        m_recordReader.records.insert(
            12, fixtures::storedRecord(12, domain::AcquisitionType::Adquisicion));
        startPresenter(domain::UserRole::Admin, 12);
        QCOMPARE(m_presenter->folio(), 12);
        QCOMPARE(m_record.title, QStringLiteral("Folio 12 · Nissan Versa 2020"));
        QCOMPARE(m_record.shownStep, 0);
        QCOMPARE(m_record.states,
                 (QList<StepVisual>{StepVisual::Current, StepVisual::Pending, StepVisual::Pending}));
    }

    // #28: el expediente se abre en consulta, con los tres pasos en solo
    // lectura y sin campos bloqueados.
    void opensWithEveryStepReadOnly()
    {
        startPresenter();
        QCOMPARE(m_details.readOnly, std::optional(true));
        QCOMPARE(m_conditions.readOnly, std::optional(true));
        QCOMPARE(m_filesView.readOnly, std::optional(true));
        QVERIFY(m_details.lockedFields.isEmpty());
    }

    // #30: una sola lectura trae los catálogos y el expediente, y cada paso
    // pinta lo suyo.
    void readsCatalogsAndRecordInOneTask()
    {
        startPresenter();
        QCOMPARE(m_runner.submitted, 1);
        QCOMPARE(m_details.setLookupsCalls, 1);
        QCOMPARE(m_conditions.setChecklistCalls, 1);
        QVERIFY(!m_record.busy);
        QVERIFY(m_record.message.isEmpty());

        const application::VehicleRecordDto stored = m_recordReader.records.value(7);
        QCOMPARE(m_details.shownFolio, std::optional(7));
        QCOMPARE(m_details.shown->serialNumber, stored.details.serialNumber);
        QCOMPARE(m_details.shown->counterparty.fullName, stored.details.counterparty.fullName);
        QCOMPARE(m_conditions.shown->inspection.size(), 1);
        QCOMPARE(m_filesView.shownImages.size(), 2);
        QVERIFY(m_filesView.shownImages.at(1).isPrimary);
        QVERIFY(!m_filesView.shownImages.at(1).bytes.isEmpty());
        QCOMPARE(m_filesView.shownDocuments.size(), 1);
        // La factura guardada es la adjunta.
        QCOMPARE(m_details.acquisition.attachment.fileLabel, QStringLiteral("factura.pdf"));
        QCOMPARE(m_presenter->details().dto().invoiceFilePath, stored.details.invoiceFilePath);
    }

    // Releer trae solo el expediente: los catálogos ya llegaron completos.
    void refreshRereadsOnlyTheRecord()
    {
        startPresenter();
        m_recordReader.records[7].details.model = QStringLiteral("Sentra");
        m_presenter->refresh();
        QCOMPARE(m_runner.submitted, 2);
        QCOMPARE(m_details.setLookupsCalls, 1);
        QCOMPARE(m_details.shown->model, QStringLiteral("Sentra"));
        QCOMPARE(m_record.title, QStringLiteral("Folio 7 · Nissan Sentra 2020"));
    }

    // Una respuesta vieja que llega tarde (o fuera de orden) no pisa a la
    // nueva.
    void staleReadsAreDropped()
    {
        createServices();
        fakes::DeferredTaskRunner deferred;
        presentation::VehicleRecordPresenter presenter(m_record, m_details, m_conditions,
                                                       m_filesView, *m_forms, *m_records, deferred,
                                                       7, domain::UserRole::Admin);
        presenter.start();
        QVERIFY(m_record.busy);
        deferred.works.at(0)(); // la primera lectura ve "Versa"...
        m_recordReader.records[7].details.model = QStringLiteral("Sentra");
        presenter.refresh();
        deferred.finish(1);     // ...la segunda, "Sentra", y llega primero
        QCOMPARE(m_details.shown->model, QStringLiteral("Sentra"));
        QVERIFY(!m_record.busy);

        deferred.dones.at(0)(); // la vieja llega al final: se descarta
        QCOMPARE(m_details.shown->model, QStringLiteral("Sentra"));
        QCOMPARE(m_details.showDetailsCalls, 1);
    }

    // Sin el checklist completo, los catálogos se vuelven a pedir en la
    // siguiente lectura; ya completos, no.
    void incompleteCatalogsAreAskedAgain()
    {
        m_reference.checklistError = QStringLiteral("relation does not exist");
        startPresenter();
        QVERIFY(m_record.message.contains(QStringLiteral("catálogos")));
        QVERIFY(m_record.message.contains(QStringLiteral("relation does not exist")));
        // El expediente sí se pintó.
        QVERIFY(m_details.shown.has_value());

        m_reference.checklistError.clear();
        m_presenter->refresh();
        QCOMPARE(m_details.setLookupsCalls, 2);
        QVERIFY(m_record.message.isEmpty());
        m_presenter->refresh();
        QCOMPARE(m_details.setLookupsCalls, 2);
    }

    // Un folio que ya no existe se avisa, y el inventario de atrás se relee
    // para que deje de mostrarlo.
    void missingVehicleIsReported()
    {
        createServices();
        presentation::VehicleRecordPresenter presenter(m_record, m_details, m_conditions,
                                                       m_filesView, *m_forms, *m_records, m_runner,
                                                       99, domain::UserRole::Admin);
        QSignalSpy vehicleChanged(&presenter, &presentation::VehicleRecordPresenter::vehicleChanged);
        presenter.start();
        QVERIFY(m_record.message.contains(QStringLiteral("ya no existe")));
        QCOMPARE(vehicleChanged.size(), 1);
        QCOMPARE(vehicleChanged.first().first().toInt(), 99);
        QVERIFY(!m_details.shown.has_value());
        QVERIFY(!m_record.busy);
    }

    void failedReadIsReported()
    {
        m_recordReader.failWith = QStringLiteral("server closed the connection");
        startPresenter();
        QVERIFY(m_record.message.contains(QStringLiteral("No se pudo leer el expediente")));
        QVERIFY(m_record.message.contains(QStringLiteral("server closed the connection")));
        QVERIFY(!m_record.busy);
    }

    // Lo que el rol deja ver se decide al abrir, antes de pintar nada.
    void adminSeesTheDealAndTheHistory()
    {
        startPresenter(domain::UserRole::Admin);
        QCOMPARE(m_record.dealTermsVisible, std::optional(true));
        QCOMPARE(m_record.maintenanceHistoryVisible, std::optional(true));
        QCOMPARE(m_details.shown->counterparty.fullName,
                 m_recordReader.records.value(7).details.counterparty.fullName);
    }

    void salesAgentSeesNeitherTheDealNorTheHistory()
    {
        startPresenter(domain::UserRole::SalesAgent);
        QCOMPARE(m_record.dealTermsVisible, std::optional(false));
        QCOMPARE(m_record.maintenanceHistoryVisible, std::optional(false));
        // Ni siquiera le llega: no basta con ocultar la tarjeta.
        QVERIFY(m_details.shown->counterparty.fullName.isEmpty());
        QCOMPARE(m_details.acquisition.attachment.fileLabel, QStringLiteral("Sin archivo"));
    }

    // Sin la operación de su rama no hay qué corregir: se avisa a quien podría
    // editar, y a nadie más.
    void missingDealTermsAreExplainedToAdmins()
    {
        m_recordReader.records[7].hasDealTerms = false;
        startPresenter(domain::UserRole::Admin);
        QVERIFY(m_record.notice.contains(QStringLiteral("no editar")));

        init();
        m_recordReader.records[7].hasDealTerms = false;
        startPresenter(domain::UserRole::SalesAgent);
        QVERIFY(m_record.notice.isEmpty());
    }

    // La factura guardada se puede abrir desde el expediente, en consulta.
    void storedInvoiceCanBeViewed()
    {
        startPresenter();
        QVERIFY(m_details.acquisition.attachment.viewVisible);
        m_presenter->details().acquisitionTerms().onViewInvoice();
        QCOMPARE(m_details.acquisition.opened,
                 QStringList{m_recordReader.records.value(7).details.invoiceFilePath});

        m_details.acquisition.openSucceeds = false;
        m_presenter->details().acquisitionTerms().onViewInvoice();
        QCOMPARE(m_details.acquisition.warnings.size(), 1);
    }

    // El historial repinta solo el total, sin releer ni repintar lo demás.
    void maintenanceCostRepaintsOnlyTheTotal()
    {
        startPresenter();
        m_presenter->showMaintenanceCost(3100.0);
        QCOMPARE(m_details.maintenanceCost, std::optional(3100.0));
        QCOMPARE(m_details.showDetailsCalls, 1);
        QCOMPARE(m_runner.submitted, 1);
    }

    // El Paso 1 pinta lo guardado con su factura: la que se guardó es la
    // adjunta, y una autofactura con factura ya pasó por la solicitud.
    void storedDetailsBringTheirInvoice()
    {
        startPresenter();
        application::VehicleDetailsDto stored =
            fixtures::validRegistration(domain::AcquisitionType::Adquisicion).details;
        stored.invoiceType = domain::InvoiceType::Autofactura;
        stored.invoiceFilePath = QStringLiteral("C:/almacen/VIN1/factura.pdf");

        m_presenter->details().showStored(12, stored);
        QCOMPARE(m_details.shownFolio, std::optional(12));
        QCOMPARE(m_details.acquisition.attachment.fileLabel, QStringLiteral("factura.pdf"));
        QVERIFY(m_details.acquisition.attachment.cfdiButtonVisible);
        QVERIFY(m_details.acquisition.attachment.uploadEnabled);
        QCOMPARE(m_presenter->details().dto().invoiceFilePath, stored.invoiceFilePath);

        m_presenter->details().showMaintenanceCost(2500.0);
        QCOMPARE(m_details.maintenanceCost, std::optional(2500.0));
    }

    // Las pestañas son libres: se entra a cualquiera, hacia adelante o hacia
    // atrás, sin validar ni bloquear, y nunca llevan candado.
    void tabsAreFree()
    {
        startPresenter();
        m_presenter->onStepClicked(2);
        QCOMPARE(m_record.shownStep, 2);
        QCOMPARE(m_record.states,
                 (QList<StepVisual>{StepVisual::Pending, StepVisual::Pending, StepVisual::Current}));
        m_presenter->onStepClicked(1);
        QCOMPARE(m_record.shownStep, 1);
        QVERIFY(!m_record.states.contains(StepVisual::Locked));
        QVERIFY(m_details.markedErrors.isEmpty());
    }

    void outOfRangeTabsAreIgnored()
    {
        startPresenter();
        m_presenter->onStepClicked(5);
        m_presenter->onStepClicked(-1);
        QCOMPARE(m_record.shownStep, 0);
    }

    // En consulta no hay nada que perder: Volver cierra sin preguntar.
    void backClosesTheRecord()
    {
        startPresenter();
        QSignalSpy closed(m_presenter.get(), &presentation::VehicleRecordPresenter::closeRequested);
        QVERIFY(m_presenter->confirmLeave());
        m_presenter->onBack();
        QCOMPARE(closed.size(), 1);
        QCOMPARE(m_record.discardQuestions, 0);
    }

    // --- Edición (US-03.4) ---

    // "Editar" solo para el Administrador, y en consulta.
    void editIsOfferedOnlyToAdmins()
    {
        startPresenter(domain::UserRole::Admin);
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QVERIFY(m_record.canEdit);
        QVERIFY(m_record.editEnabled);

        init();
        startPresenter(domain::UserRole::SalesAgent);
        QVERIFY(!m_record.canEdit);
        m_presenter->onEdit();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QCOMPARE(m_details.readOnly, std::optional(true));
    }

    // Sin el checklist completo (guardar borraría la inspección) o sin la
    // operación de su rama, no se edita.
    void editNeedsCompleteCatalogsAndDealTerms()
    {
        m_reference.checklistError = QStringLiteral("relation does not exist");
        startPresenter();
        QVERIFY(m_record.canEdit);
        QVERIFY(!m_record.editEnabled);
        m_presenter->onEdit();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));

        init();
        m_recordReader.records[7].hasDealTerms = false;
        startPresenter();
        QVERIFY(!m_record.editEnabled);
    }

    // Mientras se relee, "Editar" se deshabilita sin agrisar la página.
    void editWaitsForAReread()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        QVERIFY(!m_record.editEnabled);
        deferred.finish(0);
        QVERIFY(m_record.editEnabled);

        m_presenter->refresh();
        QVERIFY(!m_record.editEnabled);
        QVERIFY(!m_record.busy);
        deferred.finish(1);
        QVERIFY(m_record.editEnabled);
    }

    // Editar desbloquea los tres pasos, salvo la rama y el total de
    // mantenimientos.
    void editUnlocksAllButTheLockedFields()
    {
        startPresenter();
        m_presenter->onEdit();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::Edit));
        QCOMPARE(m_details.readOnly, std::optional(false));
        QCOMPARE(m_details.lockedFields,
                 (QStringList{QStringLiteral("acquisitionType"), QStringLiteral("maintenanceCost")}));
        QCOMPARE(m_conditions.readOnly, std::optional(false));
        QCOMPARE(m_filesView.readOnly, std::optional(false));
    }

    // Sin cambios, Guardar no valida ni guarda: las reglas de hoy no deben
    // marcar errores en datos que nadie tocó.
    void savingWithoutChangesSkipsValidation()
    {
        m_recordReader.records[7].details.model.clear(); // hoy no pasaría
        startPresenter();
        m_presenter->onEdit();
        // Lo que el presenter pinta (o los campos en consulta) no es un cambio.
        m_presenter->showMaintenanceCost(4000.0);
        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QCOMPARE(m_record.notice, QStringLiteral("No hubo cambios que guardar."));
        QVERIFY(m_details.markedErrors.isEmpty());
        QCOMPARE(m_vehicles.updates, 0);
        QCOMPARE(m_details.readOnly, std::optional(true));
    }

    void editsInViewModeDoNotCount()
    {
        startPresenter();
        m_presenter->onStepEdited(0);
        m_presenter->onEdit();
        m_presenter->onSave();
        QCOMPARE(m_record.notice, QStringLiteral("No hubo cambios que guardar."));
    }

    // Lo inválido se queda en edición, con sus campos marcados y en su
    // pestaña.
    void invalidChangesStayInEdit()
    {
        startPresenter();
        m_presenter->onStepClicked(2);
        editModel(QString());
        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::Edit));
        QCOMPARE(m_record.shownStep, 0);
        QVERIFY(m_details.markedFields().contains(QStringLiteral("model")));
        QVERIFY(m_record.message.startsWith(QStringLiteral("Revisa los campos marcados")));
        QCOMPARE(m_details.focusRequests.first(), QStringLiteral("model"));
        QCOMPARE(m_vehicles.updates, 0);

        // Corregido, la revalidación en vivo limpia la marca y el aviso.
        m_details.captured.model = QStringLiteral("Versa");
        m_presenter->onStepEdited(0);
        m_presenter->revalidateEditedSteps();
        QVERIFY(m_details.markedErrors.isEmpty());
        QVERIFY(m_record.message.isEmpty());
    }

    // Guardado: el inventario se relee, el expediente también, y vuelve a
    // consulta.
    void savingRereadsAndReturnsToView()
    {
        startPresenter();
        QSignalSpy changed(m_presenter.get(), &presentation::VehicleRecordPresenter::vehicleChanged);
        const qsizetype reads = m_recordReader.reads.size();
        editModel(QStringLiteral("Sentra"));
        m_presenter->onSave();

        QCOMPARE(m_vehicles.updates, 1);
        QCOMPARE(m_vehicles.updatedVersion, QStringLiteral("1001"));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QCOMPARE(m_record.notice, QStringLiteral("Cambios guardados."));
        QCOMPARE(m_details.readOnly, std::optional(true));
        // Una lectura para revisar la versión al guardar y otra para releer.
        QCOMPARE(m_recordReader.reads.size(), reads + 2);
        QVERIFY(m_record.editEnabled);
    }

    // Si la relectura falla después de guardar, se avisa y no se edita con
    // la versión vieja.
    void failedRereadAfterSavingBlocksEditing()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        deferred.finish(0);
        editModel(QStringLiteral("Sentra"));
        m_presenter->onSave();
        QVERIFY(m_record.busy);
        deferred.finish(1); // guarda
        m_recordReader.failWith = QStringLiteral("server closed the connection");
        deferred.finish(2); // relee
        QVERIFY(m_record.message.contains(QStringLiteral("Los cambios se guardaron")));
        QVERIFY(!m_record.editEnabled);
    }

    // Quitar la factura guardada (pasar a Autofactura la aparta) pregunta
    // antes, porque su archivo se borraría.
    void removingTheInvoiceAsksFirst()
    {
        startPresenter();
        m_presenter->onEdit();
        m_details.captured.invoiceType = domain::InvoiceType::Autofactura;
        m_presenter->details().onInvoiceTypeChanged();
        m_presenter->onStepEdited(0);

        m_record.removeInvoice = false;
        m_presenter->onSave();
        QCOMPARE(m_record.invoiceQuestions, QStringList{QStringLiteral("factura.pdf")});
        QCOMPARE(m_vehicles.updates, 0);
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::Edit));

        m_record.removeInvoice = true;
        m_presenter->onSave();
        QCOMPARE(m_vehicles.updates, 1);
        QVERIFY(m_vehicles.updatedInvoicePath.isEmpty());
    }

    // Lo que la base rechaza (un VIN de otra unidad) va a su campo.
    void rejectedSaveMarksTheOwningStep()
    {
        startPresenter();
        m_presenter->onStepClicked(1);
        editModel(QStringLiteral("Sentra"));
        m_vehicles.existingSerialNumbers << m_details.captured.serialNumber;
        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::Edit));
        QCOMPARE(m_record.shownStep, 0);
        QVERIFY(m_details.markedFields().contains(QStringLiteral("serialNumber")));
        QCOMPARE(m_record.states.at(0), StepVisual::Current);
    }

    // Si otro usuario la cambió, se cargan sus datos y se dice que los
    // cambios no se guardaron.
    void conflictLoadsTheirData()
    {
        startPresenter();
        editModel(QStringLiteral("Sentra"));
        m_recordReader.records[7].version = QStringLiteral("2002");
        m_recordReader.records[7].details.model = QStringLiteral("March");
        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QVERIFY(m_record.message.contains(QStringLiteral("Otro usuario")));
        QCOMPARE(m_details.shown->model, QStringLiteral("March"));
        QCOMPARE(m_vehicles.updates, 0);
    }

    // Guardar sin cambios también pinta la relectura que llegó a media
    // edición, así lo siguiente que se edite viaja con la versión nueva.
    void savingWithoutChangesAppliesAPendingReread()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        deferred.finish(0);
        m_presenter->onEdit();
        m_recordReader.records[7].version = QStringLiteral("2002");
        m_recordReader.records[7].details.model = QStringLiteral("March");
        m_presenter->refresh();
        deferred.finish(1); // llega a media edición: se guarda para después
        QCOMPARE(m_details.shown->model, QStringLiteral("Versa"));

        m_presenter->onSave(); // sin cambios
        QCOMPARE(m_record.notice, QStringLiteral("No hubo cambios que guardar."));
        QCOMPARE(m_details.shown->model, QStringLiteral("March"));

        editModel(QStringLiteral("Sentra"));
        m_presenter->onSave();
        deferred.finish(2);
        QCOMPARE(m_vehicles.updatedVersion, QStringLiteral("2002"));
    }

    // Una relectura que termina a medio guardado no quita el ocupado: lo que
    // se tecleara ahí no viajaría en el guardado y se perdería.
    void rereadDuringSaveKeepsTheRecordBusy()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        deferred.finish(0);
        editModel(QStringLiteral("Sentra"));
        m_presenter->onSave(); // tarea 1: guardar
        QVERIFY(m_record.busy);
        m_presenter->refresh(); // tarea 2: releer
        deferred.finish(2);
        QVERIFY(m_record.busy);
        deferred.finish(1);
        QVERIFY(!m_record.busy);
        QCOMPARE(m_record.notice, QStringLiteral("Cambios guardados."));
    }

    // Tras un conflicto no se deja en pantalla lo rechazado, y si la
    // relectura falla no se dice que se cargaron los datos del otro, ni se
    // edita sobre la versión vieja.
    void conflictWithAFailedRereadKeepsEditingBlocked()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        deferred.finish(0);
        editModel(QStringLiteral("Sentra"));
        m_recordReader.records[7].version = QStringLiteral("2002");
        m_presenter->onSave();
        deferred.finish(1); // conflicto: pide releer
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QCOMPARE(m_details.shown->model, QStringLiteral("Versa"));
        QVERIFY(m_record.message.contains(QStringLiteral("no se guardaron")));
        QVERIFY(!m_record.editEnabled);

        m_recordReader.failWith = QStringLiteral("server closed the connection");
        deferred.finish(2);
        QVERIFY(m_record.message.contains(QStringLiteral("Tampoco se pudieron leer")));
        QVERIFY(!m_record.message.contains(QStringLiteral("Se cargaron")));
        QVERIFY(!m_record.editEnabled);
    }

    // Si falló el COMMIT no se sabe si quedó: en vez de quedarse a
    // reintentar sobre la versión vieja (daría un conflicto falso), relee.
    void unconfirmedSaveRereadsInsteadOfRetrying()
    {
        startPresenter();
        QSignalSpy changed(m_presenter.get(), &presentation::VehicleRecordPresenter::vehicleChanged);
        editModel(QStringLiteral("Sentra"));
        application::VehicleRepository::UpdateOutcome failure;
        failure.commitFailed = true;
        failure.errorMessage = QStringLiteral("could not commit");
        m_vehicles.updateOutcome = failure;
        const qsizetype reads = m_recordReader.reads.size();

        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QVERIFY(m_record.message.contains(QStringLiteral("No se pudo confirmar")));
        QVERIFY(m_record.message.contains(QStringLiteral("revisa si tus cambios están")));
        QCOMPARE(m_recordReader.reads.size(), reads + 2); // revisar la versión y releer
        QCOMPARE(changed.size(), 1);
        QVERIFY(m_record.editEnabled);
    }

    // Disco o base: se queda en edición, con lo capturado, para reintentar.
    void failedSaveStaysInEdit()
    {
        startPresenter();
        editModel(QStringLiteral("Sentra"));
        application::VehicleRepository::UpdateOutcome failure;
        failure.errorMessage = QStringLiteral("server closed the connection");
        m_vehicles.updateOutcome = failure;
        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::Edit));
        QVERIFY(m_record.message.contains(QStringLiteral("server closed the connection")));
        QCOMPARE(m_details.captured.model, QStringLiteral("Sentra"));
        QVERIFY(!m_record.busy);
    }

    void vanishedVehicleEndsTheEdit()
    {
        startPresenter();
        QSignalSpy changed(m_presenter.get(), &presentation::VehicleRecordPresenter::vehicleChanged);
        editModel(QStringLiteral("Sentra"));
        m_recordReader.records.remove(7);
        m_presenter->onSave();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QVERIFY(m_record.message.contains(QStringLiteral("ya no existe")));
        QCOMPARE(changed.size(), 1);
        QVERIFY(!m_record.editEnabled);
    }

    // Cancelar pregunta solo si hay cambios, y vuelve a lo guardado desde
    // memoria, sin ir a la base.
    void cancelAsksOnlyWithChanges()
    {
        startPresenter();
        m_presenter->onEdit();
        m_presenter->onCancel();
        QCOMPARE(m_record.discardQuestions, 0);
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));

        editModel(QStringLiteral("Sentra"));
        m_record.discard = false;
        m_presenter->onCancel();
        QCOMPARE(m_record.discardQuestions, 1);
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::Edit));

        const qsizetype reads = m_recordReader.reads.size();
        const int painted = m_details.showDetailsCalls;
        m_record.discard = true;
        m_presenter->onCancel();
        QCOMPARE(m_record.mode, std::optional(presentation::FormMode::View));
        QCOMPARE(m_details.showDetailsCalls, painted + 1);
        QCOMPARE(m_details.captured.model, QStringLiteral("Versa"));
        QCOMPARE(m_recordReader.reads.size(), reads);
        QCOMPARE(m_details.readOnly, std::optional(true));
    }

    // Volver a consulta limpia los errores marcados y las pestañas en rojo.
    void cancelClearsTheErrors()
    {
        startPresenter();
        editModel(QString());
        m_presenter->onSave();
        QVERIFY(!m_details.markedErrors.isEmpty());
        m_presenter->onCancel();
        QVERIFY(m_details.markedErrors.isEmpty());
        QVERIFY(m_record.message.isEmpty());
        QVERIFY(!m_record.states.contains(StepVisual::Error));
    }

    // Una relectura que llega a media edición no pisa lo capturado: se
    // pinta al volver a consulta.
    void rereadDuringEditWaitsForView()
    {
        startPresenter();
        editModel(QStringLiteral("Sentra"));
        const int painted = m_details.showDetailsCalls;
        m_recordReader.records[7].details.color = QStringLiteral("Azul");
        m_presenter->refresh();
        QCOMPARE(m_details.showDetailsCalls, painted);
        QCOMPARE(m_details.captured.model, QStringLiteral("Sentra"));

        m_presenter->onCancel();
        QCOMPARE(m_details.shown->color, QStringLiteral("Azul"));
    }

    // Salir pregunta solo con cambios; mientras se guarda, pide esperar.
    void leavingAsksOnlyWithChanges()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        deferred.finish(0);
        m_presenter->onEdit();
        QVERIFY(m_presenter->confirmLeave());
        QCOMPARE(m_record.discardQuestions, 0);

        m_details.captured.model = QStringLiteral("Sentra");
        m_presenter->onStepEdited(0);
        m_record.discard = false;
        QVERIFY(!m_presenter->confirmLeave());
        QCOMPARE(m_record.discardQuestions, 1);

        m_presenter->onSave();
        QVERIFY(!m_presenter->confirmLeave());
        QVERIFY(m_record.message.contains(QStringLiteral("Espera")));
        QCOMPARE(m_record.discardQuestions, 1);
    }

    // Cancelar el diálogo de la factura no es un cambio; elegir una, sí.
    void invoiceDialogOnlyCountsWithAFile()
    {
        startPresenter();
        m_presenter->onEdit();
        m_presenter->onBrowseInvoice();
        m_presenter->onSave();
        QCOMPARE(m_record.notice, QStringLiteral("No hubo cambios que guardar."));

        m_presenter->onEdit();
        m_details.acquisition.nextInvoiceFile = QStringLiteral("C:/nuevas/factura2.pdf");
        m_presenter->onBrowseInvoice();
        m_presenter->onSave();
        QCOMPARE(m_vehicles.updates, 1);
    }

    // Si el expediente se cierra con una lectura o un guardado en curso, la
    // respuesta ya no se entrega.
    void closingWithWorkInFlightIsSafe()
    {
        fakes::DeferredTaskRunner deferred;
        startWith(deferred);
        deferred.finish(0);
        editModel(QStringLiteral("Sentra"));
        m_presenter->onSave();
        m_presenter.reset();
        deferred.finish(1);
        QCOMPARE(m_vehicles.updates, 1);
    }
};

QTEST_GUILESS_MAIN(TstVehicleRecordPresenter)
#include "tst_vehiclerecordpresenter.moc"
