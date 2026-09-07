#include "../../include/vehiclewizard/vehiclewizardview.h"
#include "../../include/contract/contractpdfgenerator.h"
#include "../../include/db/vehicleregistrationworker.h"
#include "../../include/vehiclewizard/steps/step1detailsview.h"
#include "../../include/vehiclewizard/steps/step2conditionview.h"
#include "../../include/vehiclewizard/steps/step3filesview.h"
#include "../../include/vehiclewizard/components/wizardstepper.h"

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

namespace {

// Mismo criterio que main.cpp para leer .env (primero junto al ejecutable,
// luego junto al código fuente vía la macro PROJECT_SOURCE_DIR de CMake).
QString resolveStorageRoot()
{
    QFile envFile(QCoreApplication::applicationDirPath() + QStringLiteral("/.env"));
    if (!envFile.exists())
        envFile.setFileName(QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/.env"));

    QString storageRoot;
    if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&envFile);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.startsWith(QStringLiteral("STORAGE_ROOT="))) {
                storageRoot = line.mid(QStringLiteral("STORAGE_ROOT=").size()).trimmed();
                break;
            }
        }
        envFile.close();
    }

    if (storageRoot.isEmpty())
        storageRoot = QStringLiteral(PROJECT_SOURCE_DIR) + QStringLiteral("/storage");

    return storageRoot;
}

} // namespace

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
        QString errorMessage;
        if (!m_step1->validate(errorMessage)) {
            m_step1->showError(errorMessage);
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

    // Paso 3: arma el draft completo y dispara el guardado atómico.
    m_finalDraft = VehicleDraft();
    m_step1->fillDraft(m_finalDraft);
    m_step2->fillDraft(m_finalDraft);
    m_step3->fillDraft(m_finalDraft);

    setBusy(true);
    hideError();

    m_worker = new VehicleRegistrationWorker(m_finalDraft, resolveStorageRoot(), this);
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
    m_printContractButton->setEnabled(true);
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
    const QString suggestedName = QStringLiteral("contrato_%1.pdf").arg(m_finalDraft.serialNumber);
    const QString outputPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Guardar contrato"), suggestedName, QStringLiteral("PDF (*.pdf)"));
    if (outputPath.isEmpty())
        return;

    const ContractPdfGenerator::Result result = ContractPdfGenerator::generate(m_finalDraft, outputPath);
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
