// Pruebas del presenter del asistente de registro con vistas falsas y el
// servicio real sobre puertos falsos: la navegación, la validación en vivo y
// el registro se prueban sin ventanas, sin base y sin disco.

#include "application/services/vehicleregistrationservice.h"
#include "fakes.h"
#include "presentation/presenters/vehicledetailspresenter.h"
#include "presentation/presenters/vehiclefilespresenter.h"
#include "presentation/presenters/vehiclewizardpresenter.h"
#include "registrationfixtures.h"
#include "wizardfakes.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using presentation::StepVisual;

class TstVehicleWizardPresenter : public QObject
{
    Q_OBJECT

private:
    fakes::InMemoryVehicleRepository m_vehicles;
    fakes::FakeFileStorage m_files;
    fakes::FakeReferenceDataReader m_reference;
    fakes::FakeContractGenerator m_contracts;
    fakes::InlineTaskRunner m_runner;

    fakes::FakeWizardView m_wizard;
    fakes::FakeDetailsView m_details;
    fakes::FakeConditionsView m_conditions;
    fakes::FakeFilesView m_filesView;

    std::unique_ptr<application::VehicleRegistrationService> m_service;
    std::unique_ptr<presentation::VehicleWizardPresenter> m_presenter;

    void startPresenter(bool freeNavigation = false)
    {
        m_service = std::make_unique<application::VehicleRegistrationService>(
            m_vehicles, m_files, m_reference, m_contracts);
        m_presenter = std::make_unique<presentation::VehicleWizardPresenter>(
            m_wizard, m_details, m_conditions, m_filesView, *m_service, m_runner, freeNavigation);
        m_presenter->start();
    }

    void captureValidSteps()
    {
        const application::VehicleRegistrationDto valid =
            fixtures::validRegistration(domain::AcquisitionType::Adquisicion);
        m_details.captured = valid.details;
        m_conditions.captured = valid.conditions;
        m_filesView.captured = valid.files;
    }

private slots:
    void init()
    {
        m_presenter.reset();
        m_service.reset();
        m_vehicles = fakes::InMemoryVehicleRepository();
        m_files = fakes::FakeFileStorage();
        m_reference = fakes::FakeReferenceDataReader();
        m_contracts = fakes::FakeContractGenerator();
        m_wizard = fakes::FakeWizardView();
        m_details = fakes::FakeDetailsView();
        m_conditions = fakes::FakeConditionsView();
        m_filesView = fakes::FakeFilesView();
    }

    // Al abrir: los catálogos llegan, nada se pinta en rojo, el foco no se
    // mueve y los pasos siguientes quedan bloqueados.
    void opensSilentlyWithLaterStepsLocked()
    {
        startPresenter();
        QCOMPARE(m_wizard.shownStep, 0);
        QVERIFY(!m_wizard.busy);
        QCOMPARE(m_wizard.primaryAction, presentation::PrimaryAction::Next);
        QCOMPARE(m_wizard.indicators.size(), 3);
        QCOMPARE(m_wizard.indicators.at(1).visual, StepVisual::Locked);
        QCOMPARE(m_wizard.indicators.at(2).visual, StepVisual::Locked);
        QCOMPARE(m_wizard.indicators.at(1).hint, QStringLiteral("Completa «Detalles» para continuar"));
        QCOMPARE(m_details.showErrorsCalls, 0);
        QVERIFY(m_details.focusRequests.isEmpty());
        QVERIFY(!m_details.lookups.brands.isEmpty());
        QVERIFY(!m_conditions.fuelTypes.isEmpty());
        QVERIFY(m_filesView.imageFormats.extensions.contains(QStringLiteral("png")));
    }

    void nextWithInvalidDetailsStaysAndExplains()
    {
        startPresenter();
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 0);
        QVERIFY(m_details.markedFields().contains(QStringLiteral("serialNumber")));
        QVERIFY(!m_details.focusRequests.isEmpty());
        QVERIFY(m_wizard.message.startsWith(QStringLiteral("Revisa los campos marcados")));
        QCOMPARE(m_wizard.indicators.at(0).visual, StepVisual::Current);
        QVERIFY(!m_wizard.indicators.at(0).complete);
    }

    void nextWithValidDetailsAdvancesWithCheckmark()
    {
        captureValidSteps();
        startPresenter();
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 1);
        QVERIFY(m_wizard.indicators.at(0).complete);
        QVERIFY(m_wizard.message.isEmpty());
        QVERIFY(m_details.markedErrors.isEmpty());
    }

    // Un clic hacia adelante valida lo que hay en medio y se detiene en el
    // primer paso que falta, diciendo por qué.
    void clickingAheadStopsAtFirstInvalidStep()
    {
        captureValidSteps();
        m_conditions.captured = application::VehicleConditionsDto();
        startPresenter();
        m_presenter->onStepClicked(2);
        QCOMPARE(m_wizard.shownStep, 1);
        QVERIFY(m_wizard.message.startsWith(QStringLiteral("Completa «Condición» para continuar")));
        QVERIFY(m_conditions.markedFields().contains(QStringLiteral("conditions.transmission")));
    }

    void goingBackNeedsNoValidation()
    {
        captureValidSteps();
        startPresenter();
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 1);
        m_conditions.captured = application::VehicleConditionsDto();
        m_presenter->onStepClicked(0);
        QCOMPARE(m_wizard.shownStep, 0);
        QVERIFY(m_wizard.message.isEmpty());
    }

    // La palomita sigue a la validez: corregir la limpia en vivo y la
    // enciende; romperlo después la apaga y vuelve a bloquear.
    void editingRevalidatesTheAttemptedStep()
    {
        startPresenter();
        m_presenter->onPrimaryAction();
        QVERIFY(!m_details.markedErrors.isEmpty());

        captureValidSteps();
        m_presenter->onStepEdited(0);
        m_presenter->revalidateEditedSteps();
        QVERIFY(m_details.markedErrors.isEmpty());
        QVERIFY(m_wizard.indicators.at(0).complete);
        QVERIFY(m_wizard.message.isEmpty());
        QCOMPARE(m_wizard.indicators.at(1).visual, StepVisual::Pending);

        m_details.captured.serialNumber.clear();
        m_presenter->onStepEdited(0);
        m_presenter->revalidateEditedSteps();
        QVERIFY(!m_wizard.indicators.at(0).complete);
        QCOMPARE(m_wizard.indicators.at(1).visual, StepVisual::Locked);
        // El foco no se mueve en una revalidación en vivo.
        QCOMPARE(m_details.focusRequests.size(), 1);
    }

    void debounceRevalidatesAfterTheUserStops()
    {
        startPresenter();
        m_presenter->onPrimaryAction();
        captureValidSteps();
        m_presenter->onStepEdited(0);
        QTRY_VERIFY(m_wizard.indicators.at(0).complete);
    }

    void saveRegistersAndCloses()
    {
        captureValidSteps();
        startPresenter();
        QSignalSpy registered(m_presenter.get(), &presentation::VehicleWizardPresenter::vehicleRegistered);
        QSignalSpy closed(m_presenter.get(), &presentation::VehicleWizardPresenter::closeRequested);

        m_presenter->onPrimaryAction();
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 2);
        QCOMPARE(m_wizard.primaryAction, presentation::PrimaryAction::Save);
        m_presenter->onPrimaryAction();

        QCOMPARE(m_vehicles.adds, 1);
        QCOMPARE(registered.size(), 1);
        QCOMPARE(registered.at(0).at(0).toInt(), 100);
        QCOMPARE(closed.size(), 1);
        QVERIFY(m_wizard.registered);
        QVERIFY(m_wizard.canPrint);
        QCOMPARE(m_wizard.primaryAction, presentation::PrimaryAction::Saved);
        for (const presentation::StepIndicator &indicator : std::as_const(m_wizard.indicators))
            QVERIFY(indicator.complete);
    }

    // Si el usuario pide el contrato y cancela el diálogo, el asistente se
    // queda abierto con el botón disponible.
    void cancelledContractKeepsTheWizardOpen()
    {
        captureValidSteps();
        m_wizard.afterRegistration = presentation::AfterRegistration::PrintContract;
        startPresenter();
        QSignalSpy closed(m_presenter.get(), &presentation::VehicleWizardPresenter::closeRequested);
        m_presenter->onStepClicked(2);
        m_presenter->onPrimaryAction();
        QCOMPARE(closed.size(), 0);

        m_wizard.contractPath = QStringLiteral("/out/contrato.pdf");
        m_presenter->onPrintContract();
        QCOMPARE(m_contracts.generated.size(), 1);
        QCOMPARE(m_wizard.opened, QStringList{QStringLiteral("/out/contrato.pdf")});
    }

    void duplicateVinOnSaveGoesBackToDetails()
    {
        captureValidSteps();
        m_vehicles.existingSerialNumbers << m_details.captured.serialNumber;
        startPresenter();
        m_presenter->onStepClicked(2);
        QCOMPARE(m_wizard.shownStep, 2);
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 0);
        QVERIFY(m_details.markedFields().contains(QStringLiteral("serialNumber")));
        QVERIFY(!m_wizard.busy);
        QVERIFY(m_files.stored.isEmpty());
    }

    void failedSaveStaysAndAllowsRetry()
    {
        captureValidSteps();
        m_vehicles.failNextAdd = true;
        startPresenter();
        m_presenter->onStepClicked(2);
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 2);
        QVERIFY(!m_wizard.busy);
        QVERIFY(!m_wizard.message.isEmpty());
        QCOMPARE(m_wizard.primaryAction, presentation::PrimaryAction::Save);
    }

    void freeNavigationSkipsValidation()
    {
        startPresenter(true);
        m_presenter->onPrimaryAction();
        QCOMPARE(m_wizard.shownStep, 1);
        QCOMPARE(m_details.showErrorsCalls, 0);
    }

    void cancelAsksBeforeDiscarding()
    {
        startPresenter();
        QSignalSpy closed(m_presenter.get(), &presentation::VehicleWizardPresenter::closeRequested);
        m_wizard.discard = false;
        m_presenter->onCancel();
        QCOMPARE(closed.size(), 0);
        m_wizard.discard = true;
        m_presenter->onCancel();
        QCOMPARE(closed.size(), 1);
        QCOMPARE(m_wizard.discardQuestions, 2);
    }

    // La galería agrega las fotos válidas y explica las que no.
    void rejectedImagesAreExplained()
    {
        startPresenter();
        domain::FileFacts pdf;
        pdf.fileName = QStringLiteral("doc.png");
        pdf.suffix = QStringLiteral("png");
        pdf.isReadableFile = true;
        pdf.opened = true;
        pdf.size = 10;
        pdf.contentTypes << QStringLiteral("application/pdf");
        m_files.facts.insert(QStringLiteral("/fotos/doc.png"), pdf);

        m_presenter->files().onImagesChosen(
            {QStringLiteral("/fotos/auto.png"), QStringLiteral("/fotos/doc.png")});
        QCOMPARE(m_filesView.addedImages.size(), 1);
        QCOMPARE(m_filesView.addedImages.first().path, QStringLiteral("/fotos/auto.png"));
        QVERIFY(!m_filesView.addedImages.first().bytes.isEmpty());
        QVERIFY(m_filesView.galleryMessage.contains(QStringLiteral("doc.png")));
    }

    void autofacturaUnlocksUploadAfterTheCfdiRequest()
    {
        m_details.captured.invoiceType = domain::InvoiceType::Autofactura;
        startPresenter();
        QVERIFY(m_details.attachment.cfdiButtonVisible);
        QVERIFY(!m_details.attachment.uploadEnabled);

        m_presenter->details().onCfdiRequest();
        QCOMPARE(m_details.opened.size(), 1);
        QVERIFY(m_details.attachment.uploadEnabled);

        m_details.nextInvoiceFile = QStringLiteral("C:/facturas/f1.pdf");
        m_presenter->details().onBrowseInvoice();
        QCOMPARE(m_details.attachment.fileLabel, QStringLiteral("f1.pdf"));
        QCOMPARE(m_presenter->details().dto().invoiceFilePath, QStringLiteral("C:/facturas/f1.pdf"));
    }

    void failedCfdiRequestKeepsUploadLocked()
    {
        m_details.captured.invoiceType = domain::InvoiceType::Autofactura;
        m_files.temporaryError = QStringLiteral("disco lleno");
        startPresenter();
        m_presenter->details().onCfdiRequest();
        QVERIFY(!m_details.attachment.uploadEnabled);
        QCOMPARE(m_details.warnings, QStringList{QStringLiteral("disco lleno")});
    }
};

QTEST_GUILESS_MAIN(TstVehicleWizardPresenter)
#include "tst_vehiclewizardpresenter.moc"
