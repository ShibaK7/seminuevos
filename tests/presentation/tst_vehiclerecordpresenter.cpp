// Pruebas del presenter del expediente de un vehículo (US-03.3 y US-03.4) con
// vistas falsas y los servicios reales sobre puertos falsos: qué deja ver
// cada rol y cómo se navega, sin ventanas, sin base y sin disco.

#include "application/inventory/registration/services/vehiclerecordservice.h"
#include "application/inventory/registration/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "presentation/inventory/record/vehiclerecordpresenter.h"
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
    fakes::InlineTaskRunner m_runner;

    fakes::FakeRecordView m_record;
    fakes::FakeDetailsView m_details;
    fakes::FakeConditionsView m_conditions;
    fakes::FakeFilesView m_filesView;

    std::unique_ptr<application::VehicleRegistrationService> m_forms;
    std::unique_ptr<application::VehicleRecordService> m_records;
    std::unique_ptr<presentation::VehicleRecordPresenter> m_presenter;

    void startPresenter(domain::UserRole role = domain::UserRole::Admin, int folio = 7)
    {
        m_forms = std::make_unique<application::VehicleRegistrationService>(
            m_vehicles, m_files, m_reference, m_contracts);
        m_records = std::make_unique<application::VehicleRecordService>();
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
        m_record = fakes::FakeRecordView();
        m_details = fakes::FakeDetailsView();
        m_conditions = fakes::FakeConditionsView();
        m_filesView = fakes::FakeFilesView();
    }

    void opensOnDetailsWithTheFolio()
    {
        startPresenter(domain::UserRole::Admin, 12);
        QCOMPARE(m_presenter->folio(), 12);
        QVERIFY(m_record.title.contains(QStringLiteral("12")));
        QCOMPARE(m_record.shownStep, 0);
        QCOMPARE(m_record.states,
                 (QList<StepVisual>{StepVisual::Current, StepVisual::Pending, StepVisual::Pending}));
    }

    // Lo que el rol deja ver se decide al abrir, antes de pintar nada.
    void adminSeesTheDealAndTheHistory()
    {
        startPresenter(domain::UserRole::Admin);
        QCOMPARE(m_record.dealTermsVisible, std::optional(true));
        QCOMPARE(m_record.maintenanceHistoryVisible, std::optional(true));
    }

    void salesAgentSeesNeitherTheDealNorTheHistory()
    {
        startPresenter(domain::UserRole::SalesAgent);
        QCOMPARE(m_record.dealTermsVisible, std::optional(false));
        QCOMPARE(m_record.maintenanceHistoryVisible, std::optional(false));
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
