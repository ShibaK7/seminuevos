#include "presentation/views/wizard/vehiclewizardview.h"
#include "presentation/presenters/vehicledetailspresenter.h"
#include "presentation/presenters/vehiclefilespresenter.h"
#include "presentation/presenters/vehiclewizardpresenter.h"
#include "presentation/views/components/wizardstepper.h"
#include "presentation/views/wizard/vehicleconditionsview.h"
#include "presentation/views/wizard/vehicledetailsview.h"
#include "presentation/views/wizard/vehiclefilesview.h"

#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Título de cada paso, en el orden en que se muestran.
const QStringList &stepTitles()
{
    static const QStringList titles{QStringLiteral("Detalles"), QStringLiteral("Condición"),
                                    QStringLiteral("Archivos")};
    return titles;
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
{
    m_stepper = new WizardStepper(stepTitles(), this);
    // Mismo margen izquierdo que el título, para que ambos queden alineados.
    m_stepper->setContentsMargins(8, 0, 0, 0);

    m_step1 = new VehicleDetailsView(this);
    m_step2 = new VehicleConditionsView(this);
    m_step3 = new VehicleFilesView(this);

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

    m_printContractButton = new QPushButton(QStringLiteral("Imprimir Contrato"), this);
    m_printContractButton->setProperty("class", QStringLiteral("secondary"));
    m_printContractButton->setIcon(QIcon(":/icons/printer.png"));
    m_printContractButton->setIconSize(QSize(12, 12));
    m_printContractButton->setEnabled(false);

    // El texto y el ícono los pone setPrimaryAction().
    m_primaryButton = new QPushButton(this);
    m_primaryButton->setProperty("class", QStringLiteral("primary"));
    m_primaryButton->setIconSize(QSize(12, 12));

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
}

VehicleWizardView::~VehicleWizardView() = default;

VehicleDetailsView &VehicleWizardView::detailsView()
{
    return *m_step1;
}

VehicleConditionsView &VehicleWizardView::conditionsView()
{
    return *m_step2;
}

VehicleFilesView &VehicleWizardView::filesView()
{
    return *m_step3;
}

void VehicleWizardView::bind(presentation::VehicleWizardPresenter &presenter)
{
    using presentation::VehicleWizardPresenter;
    VehicleWizardPresenter *p = &presenter;

    connect(m_stepper, &WizardStepper::stepClicked, p, &VehicleWizardPresenter::onStepClicked);
    connect(m_primaryButton, &QPushButton::clicked, p, &VehicleWizardPresenter::onPrimaryAction);
    connect(m_cancelButton, &QPushButton::clicked, p, &VehicleWizardPresenter::onCancel);
    connect(m_printContractButton, &QPushButton::clicked, p,
            &VehicleWizardPresenter::onPrintContract);

    // Las páginas avisan que algo cambió; el presenter decide cuándo
    // revalidar. El Paso 3 no se vigila: sus filas se rehacen en cada carga, y
    // basta validarlo cuando se intenta.
    connect(m_step1, &VehicleDetailsView::edited, p, [p] { p->onStepEdited(0); });
    connect(m_step2, &VehicleConditionsView::edited, p, [p] { p->onStepEdited(1); });

    connect(m_step1, &VehicleDetailsView::invoiceTypeChanged, p,
            [p] { p->details().onInvoiceTypeChanged(); });
    connect(m_step1, &VehicleDetailsView::browseInvoiceRequested, p,
            [p] { p->details().onBrowseInvoice(); });
    connect(m_step1, &VehicleDetailsView::cfdiRequestRequested, p,
            [p] { p->details().onCfdiRequest(); });
    connect(m_step3, &VehicleFilesView::imagesChosen, p,
            [p](const QStringList &paths) { p->files().onImagesChosen(paths); });
    connect(m_step3, &VehicleFilesView::documentChosen, p,
            [p](const QString &type, const QString &path) { p->files().onDocumentChosen(type, path); });

    connect(p, &VehicleWizardPresenter::vehicleRegistered, this, &VehicleWizardView::vehicleRegistered);
    connect(p, &VehicleWizardPresenter::closeRequested, this, &VehicleWizardView::returnToInventory);
}

void VehicleWizardView::showStep(int step)
{
    m_stack->setCurrentIndex(step);
}

void VehicleWizardView::showStepIndicators(const QList<presentation::StepIndicator> &indicators)
{
    for (int step = 0; step < indicators.size(); ++step) {
        const presentation::StepIndicator &indicator = indicators.at(step);
        m_stepper->setStepState(step, stepStateName(indicator.visual), indicator.complete,
                                indicator.hint);
    }
}

void VehicleWizardView::showMessage(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorLabel->setVisible(!message.isEmpty());
}

void VehicleWizardView::setPrimaryAction(presentation::PrimaryAction action)
{
    using presentation::PrimaryAction;
    switch (action) {
    case PrimaryAction::Loading:
        m_primaryButton->setText(QStringLiteral("Cargando..."));
        break;
    case PrimaryAction::Next:
        m_primaryButton->setText(QStringLiteral("Siguiente"));
        break;
    case PrimaryAction::Save:
        m_primaryButton->setText(QStringLiteral("Guardar"));
        break;
    case PrimaryAction::Saving:
        m_primaryButton->setText(QStringLiteral("Guardando..."));
        break;
    case PrimaryAction::Saved:
        m_primaryButton->setText(QStringLiteral("Guardado ✓"));
        break;
    }
    // El ícono de guardar solo en "Guardar": en "Siguiente" prometería algo
    // que ese botón no hace.
    m_primaryButton->setIcon(action == PrimaryAction::Save ? QIcon(QStringLiteral(":/icons/save.png"))
                                                           : QIcon());
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
}

void VehicleWizardView::showRegistered(bool canPrintContract)
{
    // No pasa por setBusy(false): el stepper y las páginas se quedan
    // deshabilitados, porque cualquier cambio ya no llegaría a la base.
    m_primaryButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_cancelButton->setText(QStringLiteral("Volver al Inventario"));
    m_printContractButton->setEnabled(canPrintContract);
}

presentation::AfterRegistration VehicleWizardView::askAfterRegistration(int folio,
                                                                       bool canPrintContract)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("Vehículo registrado"));
    box.setText(QStringLiteral("El vehículo se guardó correctamente (folio %1).").arg(folio));

    QPushButton *printButton = nullptr;
    if (canPrintContract) {
        box.setInformativeText(QStringLiteral("¿Deseas imprimir el contrato de la operación?"));
        printButton = box.addButton(QStringLiteral("Imprimir contrato"), QMessageBox::ActionRole);
    }
    QPushButton *backButton =
        box.addButton(QStringLiteral("Volver al inventario"), QMessageBox::AcceptRole);
    box.setDefaultButton(canPrintContract ? printButton : backButton);
    box.exec();

    return (printButton && box.clickedButton() == printButton)
               ? presentation::AfterRegistration::PrintContract
               : presentation::AfterRegistration::BackToInventory;
}

QString VehicleWizardView::askContractPath(const QString &suggestedName)
{
    return QFileDialog::getSaveFileName(this, QStringLiteral("Guardar contrato"), suggestedName,
                                        QStringLiteral("PDF (*.pdf)"));
}

void VehicleWizardView::openDocument(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void VehicleWizardView::showContractError(const QString &message)
{
    QMessageBox::warning(this, QStringLiteral("Error al generar el contrato"), message);
}

bool VehicleWizardView::confirmDiscard()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Cancelar registro"),
        QStringLiteral("¿Seguro que deseas cancelar? Se perderá la información capturada."));
    return answer == QMessageBox::Yes;
}

bool VehicleWizardView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_cancelButton) {
        if (event->type() == QEvent::Enter)
            m_cancelButton->setIcon(QIcon(":/icons/cancel_white.png"));
        else if (event->type() == QEvent::Leave)
            m_cancelButton->setIcon(QIcon(":/icons/cancel_dark.png"));
    }
    return QWidget::eventFilter(watched, event);
}
