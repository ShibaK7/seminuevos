#include "../../include/vehiclewizard/vehiclewizardview.h"
#include "../../include/app/appconfig.h"
#include "../../include/contract/contractpdfgenerator.h"
#include "../../include/db/vehicleregistrationworker.h"
#include "../../include/vehiclewizard/steps/step1detailsview.h"
#include "../../include/vehiclewizard/steps/step2conditionview.h"
#include "../../include/vehiclewizard/steps/step3filesview.h"
#include "../../include/vehiclewizard/components/wizardstepper.h"
#include "domain/vehicle.h"
#include "domain/vehiclebuilder.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

VehicleWizardView::VehicleWizardView(QWidget *parent)
    : QWidget(parent)
{
    m_stepper = new WizardStepper({QStringLiteral("Detalles"), QStringLiteral("Condición"), QStringLiteral("Archivos")}, this);
    // Mismo margen izquierdo que el título, para que ambos queden alineados.
    m_stepper->setContentsMargins(8, 0, 0, 0);
    connect(m_stepper, &WizardStepper::stepClicked, this, &VehicleWizardView::onStepClicked);

    m_step1 = new Step1DetailsView(this);
    m_step2 = new Step2ConditionView(this);
    m_step3 = new Step3FilesView(this);
    m_step1->loadLookups();
    m_step2->loadLookups();

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_step1);
    m_stack->addWidget(m_step2);
    m_stack->addWidget(m_step3);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setProperty("class", QStringLiteral("error-text"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);

    m_cancelButton = new QPushButton(QStringLiteral("Cancelar"), this);
    m_cancelButton->setProperty("class", QStringLiteral("secondary"));
    connect(m_cancelButton, &QPushButton::clicked, this, &VehicleWizardView::onCancelClicked);

    m_printContractButton = new QPushButton(QStringLiteral("Imprimir Contrato"), this);
    m_printContractButton->setProperty("class", QStringLiteral("secondary"));
    m_printContractButton->setEnabled(false);
    connect(m_printContractButton, &QPushButton::clicked, this, &VehicleWizardView::onPrintContractClicked);

    m_saveButton = new QPushButton(QStringLiteral("Guardar"), this);
    m_saveButton->setProperty("class", QStringLiteral("primary"));
    connect(m_saveButton, &QPushButton::clicked, this, &VehicleWizardView::onSaveClicked);

    auto *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(m_cancelButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(m_printContractButton);
    buttonsLayout->addWidget(m_saveButton);

    auto *titleLabel = new QLabel(QStringLiteral("Registrar Vehículo"), this);
    titleLabel->setProperty("class", QStringLiteral("h1"));
    // Alinea el título con el contenido de las tarjetas de abajo (que
    // tienen su propio margen interno además del margen del layout).
    titleLabel->setContentsMargins(8, 0, 0, 8);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(m_stepper);
    mainLayout->addWidget(m_stack, 1);
    mainLayout->addWidget(m_errorLabel);
    mainLayout->addLayout(buttonsLayout);

    goToStep(0);
}

// Fuera de línea: destruir un unique_ptr<domain::Vehicle> exige la definición
// completa de Vehicle, que el header solo declara.
VehicleWizardView::~VehicleWizardView() = default;

void VehicleWizardView::goToStep(int index)
{
    m_currentStep = index;
    m_stack->setCurrentIndex(index);
    m_stepper->setCurrentStep(index);
    hideError();
}

void VehicleWizardView::onStepClicked(int index)
{
    // Se puede navegar libremente entre cualquier paso ya validado/guardado
    // al menos una vez -- no solo hacia atrás desde el paso actual, para
    // que volver a un paso anterior no bloquee el acceso a los que ya
    // estaban completos (p.ej. regresar de Archivos a Detalles no debe
    // impedir volver a Archivos después).
    if (index <= m_maxUnlockedStep)
        goToStep(index);
}

void VehicleWizardView::onSaveClicked()
{
    if (m_currentStep == 0) {
        const domain::ValidationResult validation = m_step1->validate();
        if (!validation.isValid()) {
            m_step1->showError(validation.firstMessage());
            return;
        }
        m_step1->hideError();
        m_stepper->setStepCompleted(0, true);
        m_maxUnlockedStep = std::max(m_maxUnlockedStep, 1);
        goToStep(1);
        return;
    }

    if (m_currentStep == 1) {
        m_stepper->setStepCompleted(1, true);
        m_maxUnlockedStep = std::max(m_maxUnlockedStep, 2);
        goToStep(2);
        return;
    }

    // Paso 3: se vuelven a leer los tres pasos sobre un builder nuevo. La
    // fuente de verdad son los widgets, así que volver atrás y corregir algo
    // se refleja sin necesidad de mantener nada sincronizado.
    domain::VehicleBuilder builder;
    m_step1->applyTo(builder);
    m_step2->applyTo(builder);
    m_step3->applyTo(builder);

    domain::ValidationResult validation;
    std::unique_ptr<domain::Vehicle> vehicle = builder.build(validation);
    if (!vehicle) {
        showError(validation.firstMessage());
        return;
    }
    m_vehicle = std::move(vehicle);

    setBusy(true);
    hideError();

    // clone(): el worker reescribe las rutas de los archivos a medida que los
    // copia al almacén. Si compartiera el objeto con esta vista, un reintento
    // después de un fallo de la base buscaría los archivos en su ruta ya
    // reescrita, que no existe como origen.
    m_worker = new VehicleRegistrationWorker(m_vehicle->clone(), AppConfig::storageRoot(), this);
    connect(m_worker, &VehicleRegistrationWorker::registrationSucceeded, this, &VehicleWizardView::onRegistrationSucceeded);
    connect(m_worker, &VehicleRegistrationWorker::registrationFailed, this, &VehicleWizardView::onRegistrationFailed);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();
}

void VehicleWizardView::onCancelClicked()
{
    if (m_worker && m_worker->isRunning())
        return; // el botón ya está deshabilitado por setBusy(), por si acaso.

    if (!m_registered) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Cancelar registro"),
            QStringLiteral("¿Seguro que deseas cancelar? Se perderá la información capturada."));
        if (answer != QMessageBox::Yes)
            return;
    }

    emit cancelled();
}

void VehicleWizardView::onRegistrationSucceeded(int folio)
{
    setBusy(false);
    m_registered = true;
    m_stepper->setStepCompleted(2, true);
    m_saveButton->setEnabled(false);
    m_saveButton->setText(QStringLiteral("Guardado ✓"));
    // Que decida la unidad si le corresponde contrato: hoy las dos ramas lo
    // emiten, pero una rama futura podría no hacerlo.
    m_printContractButton->setEnabled(m_vehicle && m_vehicle->canGenerateContract());
    m_cancelButton->setText(QStringLiteral("Volver al Inventario"));

    QMessageBox::information(this, QStringLiteral("Vehículo registrado"),
                              QStringLiteral("El vehículo se guardó correctamente (folio %1).").arg(folio));

    emit vehicleRegistered(folio);
}

void VehicleWizardView::onRegistrationFailed(const QString &reason)
{
    setBusy(false);
    showError(reason);
}

void VehicleWizardView::onPrintContractClicked()
{
    if (!m_vehicle)
        return;

    const QString suggestedName = QStringLiteral("contrato_%1.pdf").arg(m_vehicle->serialNumber());
    const QString outputPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Guardar contrato"), suggestedName, QStringLiteral("PDF (*.pdf)"));
    if (outputPath.isEmpty())
        return;

    const ContractPdfGenerator::Result result = ContractPdfGenerator::generate(*m_vehicle, outputPath);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("Error al generar el contrato"), result.errorMessage);
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
}

void VehicleWizardView::setBusy(bool busy)
{
    m_saveButton->setEnabled(!busy);
    m_cancelButton->setEnabled(!busy);
    m_stepper->setEnabled(!busy);
    m_saveButton->setText(busy ? QStringLiteral("Guardando...") : QStringLiteral("Guardar"));
}

void VehicleWizardView::showError(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorLabel->setVisible(true);
}

void VehicleWizardView::hideError()
{
    m_errorLabel->setVisible(false);
}
