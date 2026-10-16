#include "presentation/inventory/record/vehiclerecordview.h"
#include "ui_vehiclerecordview.h"

#include "presentation/common/components/wizardstepper.h"
#include "presentation/inventory/record/vehiclerecordpresenter.h"
#include "presentation/inventory/registration/steps/conditions/vehicleconditionsview.h"
#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "presentation/inventory/registration/steps/files/vehiclefilesview.h"

#include <QLabel>
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

    // Solo se puede volver: editar llega con US-03.4.
    ui->cancelEditButton->setVisible(false);
    ui->editButton->setVisible(false);
    ui->saveButton->setVisible(false);
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
    ui->stepper->setEnabled(!busy);
    ui->stepStack->setEnabled(!busy);
    ui->backButton->setEnabled(!busy);
}

void VehicleRecordView::setDealTermsVisible(bool visible)
{
    ui->detailsPage->setDealTermsVisible(visible);
}

void VehicleRecordView::setMaintenanceHistoryVisible(bool visible)
{
    ui->detailsPage->setMaintenanceHistoryVisible(visible);
}
