#include "presentation/inventory/record/vehiclerecordview.h"
#include "ui_vehiclerecordview.h"

#include "presentation/common/components/wizardstepper.h"
#include "presentation/inventory/record/vehiclerecordpresenter.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailspresenter.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilespresenter.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

VehicleRecordView::VehicleRecordView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VehicleRecordView)
{
    // El .ui arma el título, las pestañas (el mismo WizardStepper del
    // asistente, promovido), la pila con las tres páginas del alta, cada una en
    // su área con scroll para que quepa en una laptop chica, los dos avisos y
    // los botones.
    ui->setupUi(this);

    // Mismos márgenes que el asistente, para que el título y las pestañas
    // queden alineados con el contenido de las tarjetas. Designer no expone
    // los márgenes de contenido de un widget.
    ui->stepper->setContentsMargins(8, 0, 0, 0);
    ui->titleLabel->setContentsMargins(8, 0, 0, 8);

    // Los avisos arrancan ocultos: solo aparecen cuando hay algo que decir.
    // Texto plano a propósito (textFormat en el .ui): un mensaje puede traer
    // lo que el usuario capturó.
    ui->errorLabel->setVisible(false);
    ui->noticeLabel->setVisible(false);

    // Hasta que el presenter diga el modo, solo se puede volver.
    showMode(presentation::FormMode::View, false, false);
}

VehicleRecordView::~VehicleRecordView()
{
    // El presenter es hijo QObject de esta vista, pero guarda referencias a
    // ella y a las tres páginas. Si se quedara a que lo borre ~QObject, para
    // entonces ~QWidget ya habría destruido las páginas. Aquí, en el cuerpo del
    // destructor, todavía no se destruye ningún hijo.
    delete m_presenter;
    delete ui;
}

VehicleDetailsView &VehicleRecordView::detailsView()
{
    return *ui->detailsPage;
}

VehicleConditionsView &VehicleRecordView::conditionsView()
{
    return *ui->conditionsPage;
}

VehicleFilesView &VehicleRecordView::filesView()
{
    return *ui->filesPage;
}

void VehicleRecordView::bind(presentation::VehicleRecordPresenter &presenter)
{
    using presentation::VehicleRecordPresenter;
    VehicleRecordPresenter *p = &presenter;
    m_presenter = p;

    connect(ui->stepper, &WizardStepper::stepClicked, p, &VehicleRecordPresenter::onStepClicked);
    connect(ui->backButton, &QPushButton::clicked, p, &VehicleRecordPresenter::onBack);
    connect(ui->editButton, &QPushButton::clicked, p, &VehicleRecordPresenter::onEdit);
    connect(ui->saveButton, &QPushButton::clicked, p, &VehicleRecordPresenter::onSave);
    connect(ui->cancelEditButton, &QPushButton::clicked, p, &VehicleRecordPresenter::onCancel);

    // Las páginas avisan cada cambio; el presenter decide si cuenta (solo al
    // editar, y nunca lo que él mismo pinta). Aquí sí se vigila el Paso 3:
    // quitar una foto o un documento también es un cambio que guardar.
    connect(ui->detailsPage, &VehicleDetailsView::edited, p, [p] { p->onStepEdited(0); });
    connect(ui->conditionsPage, &VehicleConditionsView::edited, p, [p] { p->onStepEdited(1); });
    connect(ui->filesPage, &VehicleFilesView::edited, p, [p] { p->onStepEdited(2); });

    // Lo demás que piden las páginas, igual que en el alta.
    connect(ui->detailsPage, &VehicleDetailsView::invoiceTypeChanged, p,
            [p] { p->details().onInvoiceTypeChanged(); });
    connect(ui->detailsPage, &VehicleDetailsView::browseInvoiceRequested, p,
            &VehicleRecordPresenter::onBrowseInvoice);
    connect(ui->detailsPage, &VehicleDetailsView::cfdiRequestRequested, p,
            &VehicleRecordPresenter::onCfdiRequest);
    connect(ui->filesPage, &VehicleFilesView::imagesChosen, p,
            [p](const QStringList &paths) { p->files().onImagesChosen(paths); });
    connect(ui->filesPage, &VehicleFilesView::documentChosen, p,
            [p](const QString &type, const QString &path) { p->files().onDocumentChosen(type, path); });

    connect(p, &VehicleRecordPresenter::closeRequested, this, &VehicleRecordView::returnToInventory);
    connect(p, &VehicleRecordPresenter::vehicleChanged, this, &VehicleRecordView::vehicleChanged);
}

int VehicleRecordView::folio() const
{
    return m_presenter ? m_presenter->folio() : -1;
}

void VehicleRecordView::refresh()
{
    if (m_presenter)
        m_presenter->refresh();
}

bool VehicleRecordView::confirmLeave()
{
    return m_presenter ? m_presenter->confirmLeave() : true;
}

void VehicleRecordView::showTitle(const QString &title)
{
    ui->titleLabel->setText(title);
}

void VehicleRecordView::showStep(int step)
{
    ui->stepStack->setCurrentIndex(step);
}

void VehicleRecordView::showStepStates(const QList<presentation::StepVisual> &states)
{
    // Nunca palomita ni candado: en el expediente las pestañas solo dicen cuál
    // se ve y cuál tiene errores.
    for (int step = 0; step < states.size(); ++step)
        ui->stepper->setStepVisual(step, states.at(step), false, QString());
}

void VehicleRecordView::showMessage(const QString &message)
{
    ui->errorLabel->setText(message);
    ui->errorLabel->setVisible(!message.isEmpty());
}

void VehicleRecordView::showNotice(const QString &notice)
{
    ui->noticeLabel->setText(notice);
    ui->noticeLabel->setVisible(!notice.isEmpty());
}

void VehicleRecordView::setBusy(bool busy)
{
    // Editar lo habilita showMode(), que ya sabe si hay algo en curso.
    ui->stepper->setEnabled(!busy);
    ui->stepStack->setEnabled(!busy);
    ui->backButton->setEnabled(!busy);
    ui->cancelEditButton->setEnabled(!busy);
    ui->saveButton->setEnabled(!busy);
}

void VehicleRecordView::showMode(presentation::FormMode mode, bool canEdit, bool editEnabled)
{
    const bool editing = mode == presentation::FormMode::Edit;
    ui->backButton->setVisible(!editing);
    ui->editButton->setVisible(!editing && canEdit);
    ui->editButton->setEnabled(editEnabled);
    ui->cancelEditButton->setVisible(editing);
    ui->saveButton->setVisible(editing);
}

bool VehicleRecordView::confirmDiscardChanges()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Descartar cambios"),
        QStringLiteral("Hay cambios sin guardar en el expediente. ¿Descartarlos?"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return answer == QMessageBox::Discard;
}

bool VehicleRecordView::confirmInvoiceRemoval(const QString &fileName)
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Quitar factura"),
        QStringLiteral("Al guardar se quitará del expediente la factura «%1» y se borrará su "
                       "archivo. ¿Guardar de todos modos?")
            .arg(fileName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void VehicleRecordView::setDealTermsVisible(bool visible)
{
    ui->detailsPage->setDealTermsVisible(visible);
}

void VehicleRecordView::setMaintenanceHistoryVisible(bool visible)
{
    ui->detailsPage->setMaintenanceHistoryVisible(visible);
}
