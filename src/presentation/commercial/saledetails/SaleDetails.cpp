#include "presentation/commercial/saledetails/SaleDetails.h"
#include "ui_SaleDetails.h"

#include "presentation/commercial/saledetails/saledetailspresenter.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTableWidgetItem>
#include <QLocale>

namespace {

QString money(double amount)
{
    return QLocale(QLocale::Spanish, QLocale::Mexico)
        .toCurrencyString(amount, QStringLiteral("$"), 2);
}

QString displayDate(const QDate &date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd/MM/yyyy")) : QString();
}

enum PaymentColumn {
    ColPaymentFolio = 0,
    ColInstallment,
    ColPaymentMethod,
    ColScheduledDate,
    ColPaidDate,
    ColAmount,
    ColCapital,
    ColInterest,
    ColBalance,
    ColWaived,
    ColNote,
};

} // namespace

SaleDetails::SaleDetails(domain::UserRole role, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SaleDetails)
    , m_role(role)
{
    ui->setupUi(this);
    ui->saleFolioValue->setText(QStringLiteral("—"));
    ui->vehicleValue->setText(QStringLiteral("—"));
    ui->saleDateValue->setText(QStringLiteral("—"));
    ui->buyerValue->setText(QStringLiteral("—"));
    ui->errorLabel->hide();

    const bool isAdministrator = m_role == domain::UserRole::Admin;
    // El rol de la sesión es la fuente de autorización; ocultar la columna no
    // basta, así que también se bloquean sus controles para usuarios sin permiso.
    ui->updateFinancingButton->setVisible(isAdministrator);
    ui->paymentsTable->setColumnCount(11);
    ui->paymentsTable->setHorizontalHeaderLabels({
        QStringLiteral("Folio"), QStringLiteral("No"), QStringLiteral("Forma de pago"),
        QStringLiteral("Fecha programada"), QStringLiteral("Fecha de pago"),
        QStringLiteral("Monto"), QStringLiteral("Abono al capital"),
        QStringLiteral("Pago de intereses"), QStringLiteral("Saldo"),
        QStringLiteral("Condonado"), QStringLiteral("Nota")
    });
    QPalette palete = ui->paymentsTable->palette();
    palete.setColor(QPalette::Base, QColor("#f9f9f9"));          // Fila normal
    palete.setColor(QPalette::AlternateBase, QColor("#e0f2fe")); // Fila alterna
    palete.setColor(QPalette::Highlight, QColor("#3b82f6"));     // Fondo seleccionado
    palete.setColor(QPalette::HighlightedText, QColor("#ffffff")); // Texto seleccionado
    ui->paymentsTable->setPalette(palete);
    ui->paymentsTable->setProperty("class", QStringLiteral("data-table"));
    ui->paymentsTable->verticalHeader()->setVisible(false);
    ui->paymentsTable->setAlternatingRowColors(true);
    ui->paymentsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->paymentsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->paymentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->paymentsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->paymentsTable->horizontalHeader()->setStretchLastSection(false);
    ui->paymentsTable->setColumnHidden(ColWaived, !isAdministrator);

    auto* header = ui->paymentsTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Folio
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents); // No
    header->setSectionResizeMode(10, QHeaderView::ResizeToContents); // Nota

    // Las demás se estiran para ocupar el espacio libre
    for (int col = 2; col < 10; ++col)
    {
        header->setSectionResizeMode(col, QHeaderView::Stretch);
    }

    ui->addPaymentButton->setIcon(QIcon(QStringLiteral(":/icons/plus.png")));
    ui->printButton->setIcon(QIcon(QStringLiteral(":/icons/printer.png")));

    connect(ui->backButton, &QPushButton::clicked, this, &SaleDetails::returnToCommercial);
    connect(ui->detailsCompleteButton, &QPushButton::clicked,
            this, &SaleDetails::onFullDetailsRequested);
    connect(ui->updateFinancingButton, &QPushButton::clicked,
            this, &SaleDetails::onUpdateFinancingRequested);
    connect(ui->addPaymentButton, &QPushButton::clicked,
            this, &SaleDetails::onAddPaymentRequested);
    connect(ui->printButton, &QPushButton::clicked, this, &SaleDetails::onPrintRequested);
}

SaleDetails::~SaleDetails()
{
    delete m_presenter;
    delete ui;
}

void SaleDetails::bind(presentation::SaleDetailsPresenter &presenter)
{
    m_presenter = &presenter;
    connect(this, &SaleDetails::paymentWaiverChanged,
            &presenter, &presentation::SaleDetailsPresenter::setPaymentWaived);
}

void SaleDetails::showSaleDetails(const application::CommercialSaleDetailsDto &details)
{
    ui->saleFolioValue->setText(QString::number(details.folio));
    ui->vehicleValue->setText(details.vehicleName);
    ui->saleDateValue->setText(displayDate(details.saleDate));
    ui->buyerValue->setText(details.buyerName);

    ui->totalValue->setText(money(details.totalAmount));
    ui->downPaymentValue->setText(money(details.downPayment));
    ui->amountToFinanceValue->setText(money(details.amountToFinance));
    ui->paidValue->setText(money(details.paidAmount));
    ui->capitalPaidValue->setText(money(details.capitalPaid));
    ui->interestPaidValue->setText(money(details.interestPaid));
    ui->balanceDueValue->setText(money(details.balanceDue));
    ui->paymentsTitle->setText(
        QStringLiteral("Pagos efectuados (%1/%2)")
            .arg(details.paidInstallments)
            .arg(details.expectedInstallments));

    ui->paymentsTable->setRowCount(details.payments.size());
    const bool isAdministrator = m_role == domain::UserRole::Admin;
    for (qsizetype row = 0; row < details.payments.size(); ++row) {
        const application::CommercialPaymentDto &payment = details.payments.at(row);
        const auto setText = [this, row](int column, const QString &text) {
            ui->paymentsTable->setItem(row, column, new QTableWidgetItem(text));
        };
        setText(ColPaymentFolio, QString::number(payment.folio));
        setText(ColInstallment, QString::number(payment.installmentNumber));
        setText(ColPaymentMethod, payment.paymentMethod);
        setText(ColScheduledDate, displayDate(payment.scheduledDate));
        setText(ColPaidDate, displayDate(payment.paidDate));
        setText(ColAmount, money(payment.paidAmount));
        setText(ColCapital, money(payment.capitalAllocation));
        setText(ColInterest, money(payment.interestAllocation));
        setText(ColBalance, money(payment.remainingBalance));

        auto *waived = new QCheckBox(ui->paymentsTable);
        waived->setChecked(payment.waived);
        // The UI hides this column for non-admins; only the administrator can
        // change it, and the service and SQL adapter independently validate the role.
        waived->setEnabled(isAdministrator);
        ui->paymentsTable->setCellWidget(row, ColWaived, waived);
        ui->paymentsTable->setColumnHidden(ColWaived, !isAdministrator);
        connect(waived, &QCheckBox::toggled, this,
                [this, paymentFolio = payment.folio](bool checked) {
                    emit paymentWaiverChanged(paymentFolio, checked);
                });

        auto *note = new QCheckBox(ui->paymentsTable);
        note->setChecked(!payment.notes.trimmed().isEmpty());
        note->setToolTip(payment.notes);
        note->setEnabled(false);
        ui->paymentsTable->setCellWidget(row, ColNote, note);
    }

    ui->errorLabel->clear();
    ui->errorLabel->hide();
}

void SaleDetails::showSaleDetailsError(const QString &message)
{
    ui->errorLabel->setText(QStringLiteral("No se pudo cargar el detalle de la venta: %1")
                                .arg(message));
    ui->errorLabel->show();
}

void SaleDetails::onFullDetailsRequested()
{
    QMessageBox::information(
        this, QStringLiteral("Detalle completo"),
        QStringLiteral("Folio de venta: %1").arg(ui->saleFolioValue->text()));
}

void SaleDetails::onUpdateFinancingRequested()
{
    // TODO: implementar actualización de financiamiento.
    QMessageBox::information(this, QStringLiteral("Financiamiento"),
                             QStringLiteral("Funcionalidad pendiente."));
}

void SaleDetails::onAddPaymentRequested()
{
    // TODO: implementar el registro de un pago.
    QMessageBox::information(this, QStringLiteral("Agregar pago"),
                             QStringLiteral("Funcionalidad pendiente."));
}

void SaleDetails::onPrintRequested()
{
    // TODO: implementar la impresión del detalle de venta.
    QMessageBox::information(this, QStringLiteral("Imprimir"),
                             QStringLiteral("Funcionalidad pendiente."));
}
