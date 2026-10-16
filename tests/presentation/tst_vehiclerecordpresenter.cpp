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
        m_records = std::make_unique<application::VehicleRecordService>(m_recordReader, m_files);
    }

    void startPresenter(domain::UserRole role = domain::UserRole::Admin, int folio = 7)
    {
        createServices();
        m_presenter = std::make_unique<presentation::VehicleRecordPresenter>(
            m_record, m_details, m_conditions, m_filesView, *m_forms, *m_records, m_runner, folio,
            role);
        m_presenter->start();
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
    }
};

QTEST_GUILESS_MAIN(TstVehicleRecordPresenter)
#include "tst_vehiclerecordpresenter.moc"
