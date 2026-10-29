#include "salewizardview.h"
#include "src/presentation/commercial/registration/ui_salewizardview.h"
#include "presentation/commercial/registration/saledetailsview.h"

#include "adapters/contract/pdfcontractgenerator.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QUrl>

SaleWizardView::SaleWizardView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SaleWizardView)
{
    ui->setupUi(this);

    ui->goBackButton->setVisible(false);
    ui->printContractButton->setVisible(false);
    connect(ui->saveButton, &QPushButton::clicked, this, &SaleWizardView::saveSaleRequested);
    connect(ui->cancelButton, &QPushButton::clicked, this, &SaleWizardView::onCancelClicked);
    connect(ui->printContractButton, &QPushButton::clicked, this, &SaleWizardView::printContract);

    this->setupNewSaleFlow();
    ui->cancelButton->installEventFilter(this);
}

SaleWizardView::~SaleWizardView()
{
    delete ui;
}

bool SaleWizardView::eventFilter(QObject *watched, QEvent *event){
    if (watched == ui->cancelButton) {
        if (event->type() == QEvent::Enter)
            ui->cancelButton->setIcon(QIcon(":/icons/cancel_white.png"));
        else if (event->type() == QEvent::Leave)
            ui->cancelButton->setIcon(QIcon(":/icons/cancel_dark.png"));
    }
    return QWidget::eventFilter(watched, event);
}

void SaleWizardView::setupNewSaleFlow()
{
    m_detailsView = new SaleDetailsView(this);
    ui->contentScrollArea->setWidget(m_detailsView);

    ui->goBackButton->setVisible(false);
    ui->cancelButton->setVisible(true);
    ui->saveButton->setVisible(true);
    ui->printContractButton->setVisible(false);
}

void SaleWizardView::onCancelClicked()
{
    if (!confirmDiscard())
        return;

    resetToInitialState();
}

bool SaleWizardView::confirmDiscard()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("Cancelar registro"));
    msgBox.setText(QStringLiteral("¿Seguro que deseas cancelar? Se perderá la información capturada."));
    msgBox.setIcon(QMessageBox::Question);

    // Agregas los botones con el texto exacto que quieres
    QPushButton *siButton = msgBox.addButton(QStringLiteral("Sí"), QMessageBox::YesRole);
    msgBox.addButton(QStringLiteral("No"), QMessageBox::NoRole);

    msgBox.exec();

    // Retorna true si el botón presionado fue el de "Sí"
    return msgBox.clickedButton() == siButton;
}

void SaleWizardView::resetToInitialState()
{
    if (!m_detailsView)
        return;

    m_detailsView->resetForm();
    ui->errorLabel->clear();
    ui->errorLabel->setStyleSheet(QString());
    ui->printContractButton->setVisible(false);
    ui->printContractButton->setEnabled(false);
    setSaving(false);
}

SaleDetailsView &SaleWizardView::detailsView()
{
    return *m_detailsView;
}

void SaleWizardView::setSaving(bool saving)
{
    ui->saveButton->setEnabled(!saving);
    ui->saveButton->setText(saving ? QStringLiteral("Guardando…") : QStringLiteral("Guardar"));
}

void SaleWizardView::showSaveResult(bool success, int saleFolio, const QString &message)
{
    if (success) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(QStringLiteral("Venta registrada"));
        box.setText(QStringLiteral("La venta se registró correctamente (Folio %1)").arg(saleFolio));
        box.setInformativeText(QStringLiteral("¿Deseas imprimir el contrato de la operación?"));

        QPushButton *printButton = box.addButton(QStringLiteral("Sí"), QMessageBox::YesRole);
        box.addButton(QStringLiteral("Volver al inicio"), QMessageBox::AcceptRole);
        box.setDefaultButton(printButton);
        box.exec();

        if (box.clickedButton() == printButton) {
            printContract();
        }

        resetToInitialState();
        return;
    }

    ui->printContractButton->setVisible(false);
    ui->printContractButton->setEnabled(false);
    ui->errorLabel->setText(message);
    ui->errorLabel->setStyleSheet(QStringLiteral("color: #d90429;"));
}

void SaleWizardView::setupSaleDetailsFlow(bool isAdmin)
{
    m_detailsView = new SaleDetailsView(this);

    // 2. Set read-only state unless the user has admin privileges to edit
    // m_detailsView->setReadOnly(!isAdmin);
    ui->contentScrollArea->setWidget(m_detailsView);

    ui->goBackButton->setVisible(true);
    ui->cancelButton->setVisible(false);
    ui->saveButton->setVisible(isAdmin);
    ui->printContractButton->setVisible(true);
}

void SaleWizardView::setupCollectionFlow(bool isAdmin)
{

}

void SaleWizardView::printContract()
{
    if (!m_detailsView)
        return;

    const application::SaleRegistrationDto sale = m_detailsView->registrationForm();
    if (!sale.saleDate.isValid())
        return;

    const auto locale = QLocale(QLocale::Spanish, QLocale::Mexico);

    QStringList addressParts;
    addressParts << sale.buyer.streetAddress.trimmed();
    addressParts << sale.buyer.suburb.trimmed();
    addressParts << sale.buyer.locality.trimmed();
    addressParts << sale.buyer.state.trimmed();
    addressParts << sale.buyer.postalCode.trimmed();

    QStringList filteredAddressParts;
    for (const QString &part : std::as_const(addressParts)) {
        if (!part.trimmed().isEmpty())
            filteredAddressParts << part.trimmed();
    }
    const QString buyerAddress = filteredAddressParts.join(QStringLiteral(", "));

    const QString financingText = sale.paymentType == domain::PaymentType::Contado
                                      ? QStringLiteral("Pago contado con %1")
                                            .arg(sale.paymentMethod
                                                     ? domain::displayLabel(*sale.paymentMethod)
                                                     : QStringLiteral("método no definido"))
                                      : QStringLiteral("Enganche %1, financiamiento %2 durante %3 meses")
                                            .arg(locale.toString(sale.financing->downPayment, 'f', 2),
                                                 locale.toString(sale.financing->amountToFinance, 'f', 2),
                                                 QString::number(sale.financing->monthsTerm));

    domain::ContractData contract;
    contract.title = QStringLiteral("CONTRATO DE COMPRA-VENTA");
    contract.amount = sale.totalAmount;
    contract.fileNameHint = QStringLiteral("contrato_venta_%1")
                               .arg(sale.saleDate.toString(QStringLiteral("yyyyMMdd")));

    QComboBox *sellerBox = m_detailsView->findChild<QComboBox *>(QStringLiteral("sellerBox"));
    QComboBox *vehicleCombo = m_detailsView->findChild<QComboBox *>(QStringLiteral("vehicleCombo"));

    const QString sellerName = QStringLiteral("INMOBILIARIA Y DESARROLLO INDEPENDENCIA, S.A DE C.V.");
    const QString sellerAddress = QStringLiteral("CARRETERA NACIONAL MANZANA 3 LOTE 10, COL. EL MIRADOR , MARTINEZ DE LA TORRE, VERACRUZ, C.P. 93607");

    const QString vehicleText = vehicleCombo ? vehicleCombo->currentText().trimmed() : QString();
    const QString vehicleSummary = vehicleText.section(QStringLiteral(" — Folio "), 0, 0).trimmed();
    const QStringList vehicleParts = vehicleSummary.split(QRegularExpression(QStringLiteral("\\s+")),
                                                         Qt::SkipEmptyParts);
    const QString vehicleBrand = vehicleParts.isEmpty() ? QStringLiteral("Vehículo") : vehicleParts.first();
    const QString vehicleModelAndYear = vehicleParts.size() > 1
                                            ? vehicleSummary.mid(vehicleBrand.size()).trimmed()
                                            : QStringLiteral("No disponible");

    const application::SaleVehicleOptionDto selectedVehicle = m_detailsView->selectedVehicle();

    QMap<QString, QString> placeholders;
    placeholders.insert(QStringLiteral("titulo_contrato"), contract.title);
    placeholders.insert(QStringLiteral("vendedor_nombre"), sellerName);
    placeholders.insert(QStringLiteral("vendedor_domicilio"), sellerAddress);
    placeholders.insert(QStringLiteral("vendedor_identificacion"), QString());
    placeholders.insert(QStringLiteral("comprador_nombre"),
                        sale.buyer.fullName.trimmed().isEmpty() ? QStringLiteral("No disponible")
                                                                : sale.buyer.fullName.trimmed());
    placeholders.insert(QStringLiteral("comprador_domicilio"),
                        buyerAddress.isEmpty() ? QStringLiteral("No disponible") : buyerAddress);
    placeholders.insert(QStringLiteral("comprador_identificacion"),
                        sale.buyer.nationalId.trimmed().isEmpty() ? QStringLiteral("No disponible")
                                                                   : sale.buyer.nationalId.trimmed());
    placeholders.insert(QStringLiteral("marca"), vehicleBrand);
    placeholders.insert(QStringLiteral("modelo_anio"), vehicleModelAndYear);
    placeholders.insert(QStringLiteral("tipo"), QStringLiteral("Automóvil"));
    placeholders.insert(QStringLiteral("no_serie"), !selectedVehicle.serialNumber.trimmed().isEmpty()
                                                      ? selectedVehicle.serialNumber.trimmed()
                                                      : QStringLiteral("No disponible"));
    placeholders.insert(QStringLiteral("no_motor"), !selectedVehicle.motorNumber.trimmed().isEmpty()
                                                      ? selectedVehicle.motorNumber.trimmed()
                                                      : QStringLiteral("No disponible"));
    placeholders.insert(QStringLiteral("no_placas"), !selectedVehicle.plates.trimmed().isEmpty()
                                                      ? selectedVehicle.plates.trimmed()
                                                      : QStringLiteral("No disponible"));
    placeholders.insert(QStringLiteral("color"), !selectedVehicle.color.trimmed().isEmpty()
                                                      ? selectedVehicle.color.trimmed()
                                                      : QStringLiteral("No disponible"));
    placeholders.insert(QStringLiteral("condiciones_pago"), financingText);
    placeholders.insert(QStringLiteral("fecha"), sale.saleDate.toString(QStringLiteral("dd/MM/yyyy")));
    contract.placeholders = placeholders;

    if (!sale.observations.trimmed().isEmpty())
        contract.documentLines << sale.observations.trimmed();
    else
        contract.documentLines << QStringLiteral("Sin documentos registrados.");

    const QString outputPath = QFileDialog::getSaveFileName(this, QStringLiteral("Guardar contrato"),
                                                            QStringLiteral("%1.pdf").arg(contract.fileNameHint),
                                                            QStringLiteral("PDF (*.pdf)"));
    if (outputPath.isEmpty())
        return;

    PdfContractGenerator generator;
    const auto outcome = generator.generate(contract, outputPath);
    if (!outcome.ok) {
        QMessageBox::warning(this, QStringLiteral("Contrato"), outcome.errorMessage);
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
    QMessageBox::information(this, QStringLiteral("Contrato"),
                             QStringLiteral("El contrato se generó correctamente."));
}
