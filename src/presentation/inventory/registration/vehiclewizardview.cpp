#include "presentation/inventory/registration/vehiclewizardview.h"
#include "ui_vehiclewizardview.h"

#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"
#include "presentation/inventory/registration/vehiclewizardpresenter.h"
#include "presentation/common/components/wizardstepper.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"

#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

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
    , ui(new Ui::VehicleWizardView)
{
    // El .ui arma el título, el stepper (promovido, con los títulos de los
    // pasos en su propiedad `steps`), la pila con las tres páginas
    // (VehicleDetailsView, VehicleConditionsView y VehicleFilesView,
    // promovidas), el aviso de error y los botones. El texto y el ícono del
    // botón primario los pone setPrimaryAction().
    ui->setupUi(this);

    // Mismo margen izquierdo que el título, para que ambos queden alineados.
    // Designer no expone los márgenes de contenido de un widget.
    ui->stepper->setContentsMargins(8, 0, 0, 0);

    // Los botones de los pasos van primero en el orden de tabulación, antes
    // que los campos de las páginas, porque nacen antes: Qt encadena el foco
    // en el orden de creación. Por eso `steps` va en el .ui con "Translatable"
    // desmarcado. Si fuera traducible, uic lo asignaría al final de setupUi y
    // los botones nacerían después de las páginas.

    // Alinea el título con el contenido de las tarjetas de abajo (que
    // tienen su propio margen interno además del margen del layout).
    // Designer no expone los márgenes de contenido de un QLabel.
    ui->titleLabel->setContentsMargins(8, 0, 0, 8);

    // El aviso va en texto plano a propósito (textFormat en el .ui): los
    // mensajes del dominio pueden traer lo que el usuario capturó (el nombre
    // de un documento, por ejemplo), y en modo automático QLabel interpretaría
    // como HTML cualquier cosa que lo parezca. Arranca oculto: solo aparece
    // cuando showMessage() trae algo que decir.
    ui->errorLabel->setVisible(false);

    ui->cancelButton->installEventFilter(this);
}

VehicleWizardView::~VehicleWizardView()
{
    // El presenter es hijo QObject de esta vista, pero guarda referencias a
    // ella (IVehicleWizardView) y a las tres páginas. Si se quedara a que lo
    // borre ~QObject, para entonces ~QWidget ya habría destruido las páginas y
    // ya no existiría la base IVehicleWizardView, y cualquier cosa que el
    // presenter hiciera mientras muere tocaría vistas destruidas. Aquí, en el
    // cuerpo del destructor, todavía no se destruye ningún hijo. QPointer: si
    // ya lo había borrado alguien más, queda nulo y el delete no hace nada.
    delete m_presenter;
    delete ui;
}

VehicleDetailsView &VehicleWizardView::detailsView()
{
    return *ui->detailsPage;
}

VehicleConditionsView &VehicleWizardView::conditionsView()
{
    return *ui->conditionsPage;
}

VehicleFilesView &VehicleWizardView::filesView()
{
    return *ui->filesPage;
}

void VehicleWizardView::bind(presentation::VehicleWizardPresenter &presenter)
{
    using presentation::VehicleWizardPresenter;
    VehicleWizardPresenter *p = &presenter;
    m_presenter = p;

    connect(ui->stepper, &WizardStepper::stepClicked, p, &VehicleWizardPresenter::onStepClicked);
    connect(ui->primaryButton, &QPushButton::clicked, p, &VehicleWizardPresenter::onPrimaryAction);
    connect(ui->cancelButton, &QPushButton::clicked, p, &VehicleWizardPresenter::onCancel);
    connect(ui->printContractButton, &QPushButton::clicked, p,
            &VehicleWizardPresenter::onPrintContract);

    // Las páginas avisan que algo cambió; el presenter decide cuándo
    // revalidar. El Paso 3 no se vigila: sus filas se rehacen en cada carga, y
    // basta validarlo cuando se intenta.
    connect(ui->detailsPage, &VehicleDetailsView::edited, p, [p] { p->onStepEdited(0); });
    connect(ui->conditionsPage, &VehicleConditionsView::edited, p, [p] { p->onStepEdited(1); });

    connect(ui->detailsPage, &VehicleDetailsView::invoiceTypeChanged, p,
            [p] { p->details().onInvoiceTypeChanged(); });
    connect(ui->detailsPage, &VehicleDetailsView::browseInvoiceRequested, p,
            [p] { p->details().onBrowseInvoice(); });
    connect(ui->detailsPage, &VehicleDetailsView::cfdiRequestRequested, p,
            [p] { p->details().onCfdiRequest(); });
    connect(ui->filesPage, &VehicleFilesView::imagesChosen, p,
            [p](const QStringList &paths) { p->files().onImagesChosen(paths); });
    connect(ui->filesPage, &VehicleFilesView::documentChosen, p,
            [p](const QString &type, const QString &path) { p->files().onDocumentChosen(type, path); });

    connect(p, &VehicleWizardPresenter::vehicleRegistered, this, &VehicleWizardView::vehicleRegistered);
    connect(p, &VehicleWizardPresenter::closeRequested, this, &VehicleWizardView::returnToInventory);
}

void VehicleWizardView::showStep(int step)
{
    ui->stepStack->setCurrentIndex(step);
}

void VehicleWizardView::showStepIndicators(const QList<presentation::StepIndicator> &indicators)
{
    for (int step = 0; step < indicators.size(); ++step) {
        const presentation::StepIndicator &indicator = indicators.at(step);
        ui->stepper->setStepState(step, stepStateName(indicator.visual), indicator.complete,
                                indicator.hint);
    }
}

void VehicleWizardView::showMessage(const QString &message)
{
    ui->errorLabel->setText(message);
    ui->errorLabel->setVisible(!message.isEmpty());
}

void VehicleWizardView::setPrimaryAction(presentation::PrimaryAction action)
{
    using presentation::PrimaryAction;
    switch (action) {
    case PrimaryAction::Loading:
        ui->primaryButton->setText(QStringLiteral("Cargando..."));
        break;
    case PrimaryAction::Next:
        ui->primaryButton->setText(QStringLiteral("Siguiente"));
        break;
    case PrimaryAction::Save:
        ui->primaryButton->setText(QStringLiteral("Guardar"));
        break;
    case PrimaryAction::Saving:
        ui->primaryButton->setText(QStringLiteral("Guardando..."));
        break;
    case PrimaryAction::Saved:
        ui->primaryButton->setText(QStringLiteral("Guardado ✓"));
        break;
    }
    // El ícono de guardar acompaña todo el guardado: "Guardar", "Guardando..."
    // y "Guardado ✓". En "Siguiente" prometería algo que ese botón no hace, y
    // en "Cargando..." todavía no hay nada que guardar.
    const bool savingAction = action == PrimaryAction::Save || action == PrimaryAction::Saving
                              || action == PrimaryAction::Saved;
    ui->primaryButton->setIcon(savingAction ? QIcon(QStringLiteral(":/icons/save.png")) : QIcon());
}

void VehicleWizardView::setBusy(bool busy)
{
    ui->primaryButton->setEnabled(!busy);
    ui->cancelButton->setEnabled(!busy);
    ui->stepper->setEnabled(!busy);
    // Las páginas también: lo que se editara mientras el hilo guarda no
    // llegaría a la base, pero sí se quedaría en pantalla como si se hubiera
    // guardado.
    ui->stepStack->setEnabled(!busy);
}

void VehicleWizardView::showRegistered(bool canPrintContract)
{
    // No pasa por setBusy(false): el stepper y las páginas se quedan
    // deshabilitados, porque cualquier cambio ya no llegaría a la base.
    ui->primaryButton->setEnabled(false);
    ui->cancelButton->setEnabled(true);
    ui->cancelButton->setText(QStringLiteral("Volver al Inventario"));
    ui->printContractButton->setEnabled(canPrintContract);
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
    if (watched == ui->cancelButton) {
        if (event->type() == QEvent::Enter)
            ui->cancelButton->setIcon(QIcon(":/icons/cancel_white.png"));
        else if (event->type() == QEvent::Leave)
            ui->cancelButton->setIcon(QIcon(":/icons/cancel_dark.png"));
    }
    return QWidget::eventFilter(watched, event);
}
