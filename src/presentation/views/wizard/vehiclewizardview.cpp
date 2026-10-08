#include "presentation/views/wizard/vehiclewizardview.h"
#include "app/appconfig.h"
#include "adapters/contract/contractpdfgenerator.h"
#include "db/vehicleregistrationworker.h"
#include "presentation/views/wizard/vehicledetailsview.h"
#include "presentation/views/wizard/vehicleconditionsview.h"
#include "presentation/views/wizard/vehiclefilesview.h"
#include "presentation/views/components/wizardstepper.h"
#include "domain/value_objects/validationresult.h"
#include "domain/model/vehicle.h"
#include "domain/model/vehiclebuilder.h"
#include "presentation/views/support/formsupport.h"

#include <utility>

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
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Los pasos del asistente, en el orden en que se muestran.
constexpr int kDetailsStep = 0;
constexpr int kConditionStep = 1;
constexpr int kFilesStep = 2;
constexpr int kStepCount = 3;

// Cuánto se espera después de la última edición para revalidar. Lo bastante
// corto para que la palomita y el aviso sigan al usuario mientras escribe, y
// lo bastante largo para no revalidar con cada tecla: un mismo cambio además
// avisa varias veces (un spinbox y el QLineEdit que lleva dentro, por ejemplo).
constexpr int kRevalidateDelayMs = 150;

// Título de cada paso. Lo usan el stepper y los avisos de bloqueo
// ("Completa «Detalles» para continuar"), así que sale de un solo lugar.
const QStringList &stepTitles()
{
    static const QStringList titles{QStringLiteral("Detalles"), QStringLiteral("Condición"),
                                    QStringLiteral("Archivos")};
    return titles;
}

// Por qué no se puede pasar de `blockingStep`. Es el tooltip de un paso
// bloqueado y el título del aviso cuando el asistente lleva al usuario a un
// paso que no estaba viendo.
QString blockedHint(int blockingStep)
{
    if (blockingStep < 0 || blockingStep >= stepTitles().size())
        return QString();
    return QStringLiteral("Completa «%1» para continuar").arg(stepTitles().at(blockingStep));
}

// El valor de la propiedad stepState que el stepper le pasa al QSS.
QString stepStateName(presentation::StepVisual visual)
{
    switch (visual) {
    case presentation::StepVisual::Current:
        return QStringLiteral("current");
    case presentation::StepVisual::Locked:
        return QStringLiteral("locked");
    case presentation::StepVisual::Error:
        return QStringLiteral("error");
    case presentation::StepVisual::Done:
        return QStringLiteral("done");
    case presentation::StepVisual::Pending:
        break;
    }
    return QStringLiteral("pending");
}

} // namespace

VehicleWizardView::VehicleWizardView(QWidget *parent)
    : QWidget(parent)
    , m_nav(kStepCount, AppConfig::wizardFreeNavigation())
{
    Q_ASSERT(stepTitles().size() == kStepCount);

    m_stepper = new WizardStepper(stepTitles(), this);
    // Mismo margen izquierdo que el título, para que ambos queden alineados.
    m_stepper->setContentsMargins(8, 0, 0, 0);
    connect(m_stepper, &WizardStepper::stepClicked, this, &VehicleWizardView::onStepClicked);

    m_step1 = new VehicleDetailsView(this);
    m_step2 = new VehicleConditionsView(this);
    m_step3 = new VehicleFilesView(this);
    m_step1->loadLookups();
    m_step2->loadLookups();

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_step1);
    m_stack->addWidget(m_step2);
    m_stack->addWidget(m_step3);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setProperty("class", QStringLiteral("error-text"));
    m_errorLabel->setWordWrap(true);
    // Texto plano a propósito: los mensajes del dominio pueden traer lo que el
    // usuario capturó (el nombre de un documento, por ejemplo), y en modo
    // automático QLabel interpretaría como HTML cualquier cosa que lo parezca.
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setVisible(false);

    m_cancelButton = new QPushButton(QStringLiteral("Cancelar"), this);
    m_cancelButton->setProperty("class", QStringLiteral("cancelButton"));
    m_cancelButton->setIcon(QIcon(":/icons/cancel_dark.png"));
    m_cancelButton->setIconSize(QSize(10, 10));
    m_cancelButton->installEventFilter(this);
    connect(m_cancelButton, &QPushButton::clicked, this, &VehicleWizardView::onCancelClicked);

    m_printContractButton = new QPushButton(QStringLiteral("Imprimir Contrato"), this);
    m_printContractButton->setProperty("class", QStringLiteral("secondary"));
    m_printContractButton->setIcon(QIcon(":/icons/printer.png"));
    m_printContractButton->setIconSize(QSize(12, 12));
    m_printContractButton->setEnabled(false);
    connect(m_printContractButton, &QPushButton::clicked, this, &VehicleWizardView::onPrintContractClicked);

    // El texto y el ícono dependen del paso; los pone refreshPrimaryButton().
    m_primaryButton = new QPushButton(this);
    m_primaryButton->setProperty("class", QStringLiteral("primary"));
    m_primaryButton->setIconSize(QSize(12, 12));
    connect(m_primaryButton, &QPushButton::clicked, this, &VehicleWizardView::onPrimaryClicked);

    auto *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(m_cancelButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(m_printContractButton);
    buttonsLayout->addWidget(m_primaryButton);

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

    m_revalidateTimer = new QTimer(this);
    m_revalidateTimer->setSingleShot(true);
    m_revalidateTimer->setInterval(kRevalidateDelayMs);
    connect(m_revalidateTimer, &QTimer::timeout, this, &VehicleWizardView::revalidateEditedSteps);

    // Después de loadLookups() y no antes: ahí se crean widgets que también
    // hay que vigilar (el checklist del Paso 2, el editor del combo de marca
    // del Paso 1), y los combos que se llenan ahí no disparan revalidaciones
    // de arranque. El Paso 3 no se vigila: rehace sus filas en cada carga, así
    // que basta validarlo cuando se intenta.
    formsupport::watchEdits(m_step1, this, [this]() { onStepEdited(kDetailsStep); });
    formsupport::watchEdits(m_step2, this, [this]() { onStepEdited(kConditionStep); });

    // Validación inicial en silencio: sin marcar ningún paso como intentado,
    // no se pinta nada en rojo ni se mueve el foco. Solo sirve para que el
    // stepper sepa desde el principio qué pasos están bloqueados.
    for (int step = 0; step < kStepCount; ++step)
        m_nav.setValid(step, validateStep(step).isValid());

    showCurrentStep();
}

// Fuera de línea: destruir un unique_ptr<domain::Vehicle> exige la definición
// completa de Vehicle, que el header solo declara.
VehicleWizardView::~VehicleWizardView() = default;

domain::ValidationResult VehicleWizardView::validateStep(int step) const
{
    switch (step) {
    case kDetailsStep:
        return m_step1->validate();
    case kConditionStep:
        return m_step2->validate();
    case kFilesStep:
        // El Paso 3 todavía no tiene reglas propias: las fotos y los
        // documentos son opcionales. Lo que sí puede fallar ahí, como un
        // documento repetido, lo reporta build() al guardar.
        break;
    }
    return domain::ValidationResult();
}

void VehicleWizardView::showCurrentStep()
{
    m_stack->setCurrentIndex(m_nav.current());
    refreshStepper();
    refreshPrimaryButton();
}

void VehicleWizardView::refreshStepper()
{
    for (int step = 0; step < m_nav.stepCount(); ++step) {
        const presentation::StepVisual visual = m_nav.visual(step);
        // Solo un paso bloqueado lleva tooltip, y nombra el primer paso
        // anterior que falta: es el que hay que completar antes que nada.
        const QString hint = (visual == presentation::StepVisual::Locked)
                                 ? blockedHint(m_nav.firstBlockingStep(step))
                                 : QString();
        m_stepper->setStepState(step, stepStateName(visual), m_nav.isComplete(step), hint);
    }
}

void VehicleWizardView::refreshPrimaryButton()
{
    const bool lastStep = (m_nav.current() == kStepCount - 1);
    m_primaryButton->setText(lastStep ? QStringLiteral("Guardar") : QStringLiteral("Siguiente"));
    // El ícono de guardar solo en "Guardar": en "Siguiente" prometería algo
    // que ese botón no hace.
    m_primaryButton->setIcon(lastStep ? QIcon(QStringLiteral(":/icons/save.png")) : QIcon());
}

void VehicleWizardView::onStepClicked(int index)
{
    const int current = m_nav.current();
    if (index == current)
        return;

    if (index < current) {
        // Regresar siempre se puede y no valida nada: corregir un paso
        // anterior no tiene por qué exigir que el actual ya esté completo.
        m_nav.moveTo(index);
        showCurrentStep();
        hideError();
        return;
    }

    // Hacia adelante, aunque el paso se vea bloqueado: advanceTo() valida lo
    // que hay en medio y, si algo falta, dice qué.
    advanceTo(index);
}

void VehicleWizardView::onPrimaryClicked()
{
    if (m_nav.current() < kStepCount - 1) {
        advanceTo(m_nav.current() + 1);
        return;
    }

    // Guardar: se revisan los tres pasos, no solo el actual, y los tres
    // quedan como intentados. Así cualquier paso con datos pendientes se
    // marca en el stepper, aunque la navegación libre haya dejado pasar sin
    // validar.
    m_nav.markAllAttempted();
    int firstInvalidStep = -1;
    domain::ValidationResult firstInvalidResult;
    for (int step = 0; step < kStepCount; ++step) {
        domain::ValidationResult result = validateStep(step);
        m_nav.setValid(step, result.isValid());
        // Los tres quedaron como intentados, así que cada uno marca sus campos,
        // también los que no se ven: al regresar a ellos ya dicen qué falta.
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

    // Se vuelven a leer los tres pasos sobre un builder nuevo. La fuente de
    // verdad son los widgets, así que volver atrás y corregir algo se refleja
    // sin necesidad de mantener nada sincronizado.
    domain::VehicleBuilder builder;
    m_step1->applyTo(builder);
    m_step2->applyTo(builder);
    m_step3->applyTo(builder);

    domain::ValidationResult validation;
    std::unique_ptr<domain::Vehicle> vehicle = builder.build(validation);
    if (!vehicle) {
        // build() revisa también lo que ningún paso valida por su cuenta,
        // como un documento repetido en el Paso 3. Van todos los mensajes, no
        // solo el primero.
        showValidationErrors(QStringLiteral("No se pudo registrar el vehículo"), validation);
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

void VehicleWizardView::advanceTo(int target)
{
    if (target <= m_nav.current() || target >= m_nav.stepCount())
        return;

    // Navegación libre (WIZARD_FREE_NAVIGATION, solo desarrollo): se entra sin
    // validar, que es justo lo que promete la bandera. Guardar sigue
    // validando los tres pasos.
    if (m_nav.freeNavigation()) {
        m_nav.goTo(target);
        showCurrentStep();
        hideError();
        return;
    }

    for (int step = m_nav.current(); step < target; ++step) {
        const domain::ValidationResult result = validateStep(step);
        m_nav.setValid(step, result.isValid());
        m_nav.markAttempted(step);
        // Ya intentado, el paso marca sus campos. Si es válido, esto limpia lo
        // que quedara de un intento anterior sin esperar a la revalidación en
        // vivo, que corre después del debounce.
        markStepFields(step, result);
        if (!result.isValid()) {
            // Los pasos posteriores ni se validan: el usuario se queda en el
            // primero que falta y ve todo lo que le falta a ese.
            showInvalidStep(step, result);
            return;
        }
    }

    // Todo lo que hay entre el paso actual y el destino es válido, y lo
    // anterior al actual también: no se llega a un paso sin haber pasado por
    // los de antes.
    m_nav.goTo(target);
    showCurrentStep();
    hideError();
}

void VehicleWizardView::showInvalidStep(int step, const domain::ValidationResult &result)
{
    // Si el paso inválido es el que ya se ve, el aviso solo pide revisarlo. Si
    // no, el asistente lleva al usuario hasta él, y el aviso dice por qué está
    // ahí y no en el paso que pidió.
    const bool alreadyVisible = (step == m_nav.current());
    if (!alreadyVisible)
        m_nav.moveTo(step);
    showCurrentStep();
    showValidationErrors(alreadyVisible ? QStringLiteral("Revisa los campos marcados")
                                        : blockedHint(step),
                         result);

    // El foco va al primer error que tenga un campo donde mostrarse: uno que no
    // se captura en pantalla, como la UMA, no debe dejar al usuario sin cursor.
    // Solo aquí se mueve el foco, porque el usuario acaba de pedir avanzar o
    // guardar; la revalidación en vivo nunca lo toca, ya que a media captura lo
    // mandaría a teclear en otro campo.
    QWidget *page = m_stack->widget(step);
    for (const domain::ValidationError &error : result.errors()) {
        if (formsupport::focusField(page, error.field))
            break;
    }
}

void VehicleWizardView::markStepFields(int step, const domain::ValidationResult &result)
{
    // Las páginas del stack se agregaron en el orden de los pasos.
    QWidget *page = m_stack->widget(step);
    if (result.isValid())
        formsupport::clearFieldErrors(page);
    else
        formsupport::showFieldErrors(page, result.errors());
}

void VehicleWizardView::onStepEdited(int step)
{
    m_editedSteps.insert(step);
    // start() reinicia la cuenta si ya corría: la revalidación espera a que
    // el usuario deje de teclear, en vez de correr con cada cambio.
    m_revalidateTimer->start();
}

void VehicleWizardView::revalidateEditedSteps()
{
    const QSet<int> editedSteps = std::exchange(m_editedSteps, QSet<int>());
    for (int step : editedSteps) {
        const domain::ValidationResult result = validateStep(step);
        m_nav.setValid(step, result.isValid());

        // Los errores se muestran solo en un paso ya intentado: antes de eso,
        // nadie quiere ver en rojo un formulario que apenas está llenando.
        if (!m_nav.isAttempted(step))
            continue;

        // Los campos se marcan y se limpian mientras el usuario corrige, pero
        // el foco se queda donde está.
        markStepFields(step, result);

        // El aviso habla del paso que se ve.
        if (step == m_nav.current()) {
            if (result.isValid())
                hideError();
            else
                showValidationErrors(QStringLiteral("Revisa los campos marcados"), result);
        }
    }
    // La palomita y los bloqueos siguen a la validez: un paso que deja de ser
    // válido pierde la palomita y vuelve a bloquear los siguientes.
    refreshStepper();
}

void VehicleWizardView::showValidationErrors(const QString &title,
                                             const domain::ValidationResult &result)
{
    // Todos los mensajes y no solo el primero: quien captura prefiere ver de
    // una vez todo lo que le falta, en vez de descubrirlo de uno en uno.
    showError(title + QStringLiteral(":\n• ") + result.joinedMessages(QStringLiteral("\n• ")));
}

void VehicleWizardView::onCancelClicked()
{
    // m_worker es un QPointer: cuando el hilo termina, deleteLater() lo borra y
    // el puntero queda en nulo solo. Con un puntero crudo, "Volver al
    // Inventario" después de un registro leía un hilo ya destruido.
    if (m_worker && m_worker->isRunning())
        return; // el botón ya está deshabilitado por setBusy(), por si acaso.

    if (!m_registered) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Cancelar registro"),
            QStringLiteral("¿Seguro que deseas cancelar? Se perderá la información capturada."));
        if (answer != QMessageBox::Yes)
            return;
    }

    emit returnToInventory();
}

void VehicleWizardView::onRegistrationSucceeded(int folio)
{
    m_registered = true;
    // No pasa por setBusy(false): la unidad ya quedó guardada, así que el
    // stepper y las páginas se quedan deshabilitados, porque cualquier cambio
    // ya no llegaría a la base. Solo vuelven el botón para salir y, más
    // abajo, el del contrato.
    m_cancelButton->setEnabled(true);
    m_primaryButton->setText(QStringLiteral("Guardado ✓"));
    // Los tres pasos con palomita: Guardar los marcó como intentados y los
    // tres pasaron la validación.
    refreshStepper();
    // Que decida la unidad si le corresponde contrato: hoy las dos ramas lo
    // emiten, pero una rama futura podría no hacerlo.
    const bool canPrint = m_vehicle && m_vehicle->canGenerateContract();
    m_printContractButton->setEnabled(canPrint);
    m_cancelButton->setText(QStringLiteral("Volver al Inventario"));

    // El contrato se ofrece AQUÍ, que es cuando los datos de la operación
    // están completos y a la mano. Después de esto se vuelve al inventario, y
    // reconstruirlos desde la base para reimprimir seria otro trabajo.
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("Vehículo registrado"));
    box.setText(QStringLiteral("El vehículo se guardó correctamente (folio %1).").arg(folio));

    QPushButton *printButton = nullptr;
    if (canPrint) {
        box.setInformativeText(QStringLiteral("¿Deseas imprimir el contrato de la operación?"));
        printButton = box.addButton(QStringLiteral("Imprimir contrato"), QMessageBox::ActionRole);
    }
    QPushButton *backButton =
        box.addButton(QStringLiteral("Volver al inventario"), QMessageBox::AcceptRole);
    box.setDefaultButton(canPrint ? printButton : backButton);
    box.exec();

    emit vehicleRegistered(folio);

    if (box.clickedButton() == printButton && !printContract()) {
        // No se pudo generar (o se cancelo el diálogo de guardado): se deja la
        // pantalla abierta con el botón de contrato disponible, en vez de
        // volver al inventario y perder la oportunidad.
        return;
    }

    emit returnToInventory();
}

void VehicleWizardView::onRegistrationFailed(const QString &reason)
{
    setBusy(false);
    showError(reason);
}

void VehicleWizardView::onPrintContractClicked()
{
    printContract();
}

bool VehicleWizardView::printContract()
{
    if (!m_vehicle)
        return false;

    const QString suggestedName = QStringLiteral("contrato_%1.pdf").arg(m_vehicle->serialNumber());
    const QString outputPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Guardar contrato"), suggestedName, QStringLiteral("PDF (*.pdf)"));
    if (outputPath.isEmpty())
        return false;

    const ContractPdfGenerator::Result result = ContractPdfGenerator::generate(*m_vehicle, outputPath);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("Error al generar el contrato"), result.errorMessage);
        return false;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
    return true;
}

void VehicleWizardView::setBusy(bool busy)
{
    m_primaryButton->setEnabled(!busy);
    m_cancelButton->setEnabled(!busy);
    m_stepper->setEnabled(!busy);
    // Las páginas también: lo que se editara mientras el hilo guarda no
    // llegaría a la base, pero sí se quedaría en pantalla como si se hubiera
    // guardado.
    m_stack->setEnabled(!busy);
    if (busy)
        m_primaryButton->setText(QStringLiteral("Guardando..."));
    else
        refreshPrimaryButton();
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

bool VehicleWizardView::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_cancelButton) {
        if (event->type() == QEvent::Enter) {
            m_cancelButton->setIcon(QIcon(":/icons/cancel_white.png"));
        } else if (event->type() == QEvent::Leave) {
            m_cancelButton->setIcon(QIcon(":/icons/cancel_dark.png"));
        }
    }

    return QWidget::eventFilter(watched, event);
}
