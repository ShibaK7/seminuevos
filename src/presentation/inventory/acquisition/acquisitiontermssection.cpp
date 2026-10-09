#include "presentation/inventory/acquisition/acquisitiontermssection.h"
#include "ui_acquisitiontermssection.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

AcquisitionTermsSection::AcquisitionTermsSection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AcquisitionTermsSection)
{
    // El archivo de factura solo existe en la compra: el esquema pone
    // invoice_file_path únicamente en vehicle_acquisitions. Va envuelto en un
    // widget (invoiceFileWidget) para tratar la fila de una pieza. Sus botones
    // no llevan "field": abren un diálogo en vez de capturar un dato, y el
    // tooltip del de subir explica cuándo está bloqueado, algo que el tooltip
    // de un error taparía. El de CFDI va antes que el de subir porque en
    // autofactura hay que usarlo primero: leída de izquierda a derecha, la
    // fila repite la secuencia que se exige.
    ui->setupUi(this);

    // Los combos de valores cerrados se llenan desde el dominio.
    for (domain::PaymentType value : domain::allPaymentTypes())
        ui->paymentTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    for (domain::PaymentMethod value : domain::allPaymentMethods())
        ui->paymentMethodCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    // Solo aplica a la autofactura, y en ella hay que usarlo antes de poder
    // subir la factura. Arranca oculto; ni la visibilidad ni el bloqueo se
    // deciden aquí: los decide el presenter y llegan por
    // showInvoiceAttachment().
    ui->cfdiRequestButton->setVisible(false);
    connect(ui->cfdiRequestButton, &QPushButton::clicked, this,
            &AcquisitionTermsSection::cfdiRequestRequested);
    connect(ui->invoiceUploadButton, &QPushButton::clicked, this,
            &AcquisitionTermsSection::browseInvoiceRequested);
}

AcquisitionTermsSection::~AcquisitionTermsSection()
{
    delete ui;
}

void AcquisitionTermsSection::fill(application::VehicleDetailsDto &dto) const
{
    dto.purchasePrice = ui->purchasePriceSpin->value();
    dto.salePrice = ui->salePriceSpin->value();
    dto.paymentType = static_cast<domain::PaymentType>(ui->paymentTypeCombo->currentData().toInt());
    dto.paymentMethod =
        static_cast<domain::PaymentMethod>(ui->paymentMethodCombo->currentData().toInt());
}

QList<QWidget *> AcquisitionTermsSection::focusOrder() const
{
    return {ui->cfdiRequestButton, ui->invoiceUploadButton, ui->purchasePriceSpin,
            ui->paymentTypeCombo,  ui->paymentMethodCombo,  ui->salePriceSpin};
}

QGridLayout *AcquisitionTermsSection::grid() const
{
    return ui->acquisitionTermsGrid;
}

void AcquisitionTermsSection::showInvoiceAttachment(const presentation::InvoiceAttachmentState &state)
{
    // Ocultar el botón por su cuenta no choca con que la sección entera se
    // oculte en la consignación: un hijo ocultado explícitamente sigue oculto
    // cuando su padre se vuelve a mostrar, así que un mecanismo no deshace al
    // otro.
    ui->cfdiRequestButton->setVisible(state.cfdiButtonVisible);
    ui->invoiceUploadButton->setEnabled(state.uploadEnabled);
    ui->invoiceUploadButton->setToolTip(state.uploadToolTip);
    ui->invoiceFileLabel->setText(state.fileLabel);
    ui->invoiceFileLabel->setToolTip(state.fileToolTip);
}

QString AcquisitionTermsSection::askInvoiceFile()
{
    return QFileDialog::getOpenFileName(this, QStringLiteral("Seleccionar factura"));
}

bool AcquisitionTermsSection::openDocument(const QString &path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AcquisitionTermsSection::showWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}
