#include "../../../include/vehiclewizard/steps/step1detailsview.h"
#include "../../../include/db/connectionpool.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextEdit>
#include <QVBoxLayout>

Step1DetailsView::Step1DetailsView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(buildGeneralInfoCard());
    layout->addWidget(buildOwnerAndAcquisitionCard());

    m_errorLabel = new QLabel(this);
    m_errorLabel->setProperty("class", QStringLiteral("error-text"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);
    layout->addWidget(m_errorLabel);

    connect(m_vehicleTypeCombo, &QComboBox::currentIndexChanged, this, &Step1DetailsView::reloadSubtypes);
}

QWidget *Step1DetailsView::buildGeneralInfoCard()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    auto *cardLayout = new QVBoxLayout(card);
    // Folio es la primera fila del formulario -- que quede pegada arriba
    // del contenedor, con solo unos pocos px de aire, no el margen grande
    // por defecto del layout.
    cardLayout->setContentsMargins(16, 8, 16, 16);

    auto *grid = new QGridLayout;
    int row = 0;

    m_folioLabel = new QLabel(QStringLiteral("(auto)"), card);
    m_dateEdit = new QDateEdit(QDate::currentDate(), card);
    m_dateEdit->setCalendarPopup(true);
    m_vehicleTypeCombo = new QComboBox(card);
    m_subtypeCombo = new QComboBox(card);
    m_brandCombo = new QComboBox(card);
    m_modelEdit = new QLineEdit(card);
    m_modelEdit->setPlaceholderText(QStringLiteral("p.ej. Kicks Advance TM"));
    m_yearModelSpin = new QSpinBox(card);
    m_yearModelSpin->setRange(1980, QDate::currentDate().year() + 1);
    m_yearModelSpin->setValue(QDate::currentDate().year());

    grid->addWidget(new QLabel(QStringLiteral("Folio:"), card), row, 0);
    grid->addWidget(m_folioLabel, row, 1);
    grid->addWidget(new QLabel(QStringLiteral("Fecha:"), card), row, 2);
    grid->addWidget(m_dateEdit, row, 3);
    ++row;

    grid->addWidget(new QLabel(QStringLiteral("Tipo Vehículo:"), card), row, 0);
    grid->addWidget(m_vehicleTypeCombo, row, 1);
    grid->addWidget(new QLabel(QStringLiteral("Subtipo:"), card), row, 2);
    grid->addWidget(m_subtypeCombo, row, 3);
    ++row;

    grid->addWidget(new QLabel(QStringLiteral("Marca:"), card), row, 0);
    grid->addWidget(m_brandCombo, row, 1);
    grid->addWidget(new QLabel(QStringLiteral("Modelo (tipo):"), card), row, 2);
    grid->addWidget(m_modelEdit, row, 3);
    ++row;

    grid->addWidget(new QLabel(QStringLiteral("Año Modelo:"), card), row, 0);
    grid->addWidget(m_yearModelSpin, row, 1);

    m_colorEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Color:"), card), row, 2);
    grid->addWidget(m_colorEdit, row, 3);
    ++row;

    m_mileageSpin = new QSpinBox(card);
    m_mileageSpin->setRange(0, 2000000);
    m_mileageSpin->setSuffix(QStringLiteral(" km"));
    grid->addWidget(new QLabel(QStringLiteral("Kilometraje:"), card), row, 0);
    grid->addWidget(m_mileageSpin, row, 1);

    m_motorNumberEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("No. Motor:"), card), row, 2);
    grid->addWidget(m_motorNumberEdit, row, 3);
    ++row;

    m_serialNumberEdit = new QLineEdit(card);
    m_serialNumberEdit->setPlaceholderText(QStringLiteral("VIN"));
    grid->addWidget(new QLabel(QStringLiteral("No. Serie (VIN):"), card), row, 0);
    grid->addWidget(m_serialNumberEdit, row, 1);

    m_repuveEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("REPUVE:"), card), row, 2);
    grid->addWidget(m_repuveEdit, row, 3);
    ++row;

    m_platesEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Placas:"), card), row, 0);
    grid->addWidget(m_platesEdit, row, 1);

    m_platesHolderEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Titular Placas:"), card), row, 2);
    grid->addWidget(m_platesHolderEdit, row, 3);
    ++row;

    m_descriptionEdit = new QTextEdit(card);
    m_descriptionEdit->setMaximumHeight(60);
    grid->addWidget(new QLabel(QStringLiteral("Descripción:"), card), row, 0);
    grid->addWidget(m_descriptionEdit, row, 1, 1, 3);

    cardLayout->addLayout(grid);
    return card;
}

QWidget *Step1DetailsView::buildOwnerAndAcquisitionCard()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    auto *cardLayout = new QVBoxLayout(card);

    auto *grid = new QGridLayout;
    int row = 0;

    m_ownerNameEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Propietario:"), card), row, 0);
    grid->addWidget(m_ownerNameEdit, row, 1);

    m_ownerIdEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Identificación:"), card), row, 2);
    grid->addWidget(m_ownerIdEdit, row, 3);
    ++row;

    m_ownerAddressEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Domicilio:"), card), row, 0);
    grid->addWidget(m_ownerAddressEdit, row, 1);

    m_ownerSuburbEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Colonia:"), card), row, 2);
    grid->addWidget(m_ownerSuburbEdit, row, 3);
    ++row;

    m_ownerLocalityEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Localidad:"), card), row, 0);
    grid->addWidget(m_ownerLocalityEdit, row, 1);

    m_ownerStateEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Estado:"), card), row, 2);
    grid->addWidget(m_ownerStateEdit, row, 3);
    ++row;

    m_ownerPostalCodeEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Código Postal:"), card), row, 0);
    grid->addWidget(m_ownerPostalCodeEdit, row, 1);
    ++row;

    m_invoiceTypeCombo = new QComboBox(card);
    m_invoiceTypeCombo->addItems({QStringLiteral("Facturado"), QStringLiteral("Autofactura")});
    grid->addWidget(new QLabel(QStringLiteral("Tipo Factura:"), card), row, 0);
    grid->addWidget(m_invoiceTypeCombo, row, 1);

    auto *invoiceFileLayout = new QHBoxLayout;
    m_invoiceFileLabel = new QLabel(QStringLiteral("Sin archivo"), card);
    auto *invoiceFileButton = new QPushButton(QStringLiteral("Subir documento"), card);
    invoiceFileButton->setProperty("class", QStringLiteral("secondary"));
    connect(invoiceFileButton, &QPushButton::clicked, this, &Step1DetailsView::onBrowseInvoiceFile);
    invoiceFileLayout->addWidget(m_invoiceFileLabel, 1);
    invoiceFileLayout->addWidget(invoiceFileButton);
    grid->addWidget(new QLabel(QStringLiteral("Factura:"), card), row, 2);
    grid->addLayout(invoiceFileLayout, row, 3);
    ++row;

    m_invoiceNumberEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("No. Factura:"), card), row, 0);
    grid->addWidget(m_invoiceNumberEdit, row, 1);

    m_invoiceIssuerEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Expidió Factura:"), card), row, 2);
    grid->addWidget(m_invoiceIssuerEdit, row, 3);
    ++row;

    m_purchasePriceSpin = new QDoubleSpinBox(card);
    m_purchasePriceSpin->setRange(0, 99999999);
    m_purchasePriceSpin->setPrefix(QStringLiteral("$ "));
    m_purchasePriceSpin->setDecimals(2);
    grid->addWidget(new QLabel(QStringLiteral("Precio Compra:"), card), row, 0);
    grid->addWidget(m_purchasePriceSpin, row, 1);

    m_paymentTypeCombo = new QComboBox(card);
    m_paymentTypeCombo->addItems({QStringLiteral("Contado"), QStringLiteral("Crédito")});
    grid->addWidget(new QLabel(QStringLiteral("Tipo Pago:"), card), row, 2);
    grid->addWidget(m_paymentTypeCombo, row, 3);
    ++row;

    m_paymentMethodCombo = new QComboBox(card);
    m_paymentMethodCombo->addItems({QStringLiteral("Efectivo"), QStringLiteral("Transferencia")});
    grid->addWidget(new QLabel(QStringLiteral("Método de Pago:"), card), row, 0);
    grid->addWidget(m_paymentMethodCombo, row, 1);

    m_maintenanceCostSpin = new QDoubleSpinBox(card);
    m_maintenanceCostSpin->setRange(0, 9999999);
    m_maintenanceCostSpin->setPrefix(QStringLiteral("$ "));
    grid->addWidget(new QLabel(QStringLiteral("Mantenimientos:"), card), row, 2);
    grid->addWidget(m_maintenanceCostSpin, row, 3);
    ++row;

    m_salePriceSpin = new QDoubleSpinBox(card);
    m_salePriceSpin->setRange(0, 99999999);
    m_salePriceSpin->setPrefix(QStringLiteral("$ "));
    grid->addWidget(new QLabel(QStringLiteral("Precio Venta:"), card), row, 0);
    grid->addWidget(m_salePriceSpin, row, 1);
    ++row;

    m_observationsEdit = new QTextEdit(card);
    m_observationsEdit->setMaximumHeight(60);
    grid->addWidget(new QLabel(QStringLiteral("Observaciones:"), card), row, 0);
    grid->addWidget(m_observationsEdit, row, 1, 1, 3);

    cardLayout->addLayout(grid);
    return card;
}

void Step1DetailsView::onBrowseInvoiceFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Seleccionar factura"));
    if (path.isEmpty())
        return;

    m_invoiceFilePath = path;
    m_invoiceFileLabel->setText(QFileInfo(path).fileName());
}

void Step1DetailsView::reloadSubtypes()
{
    m_subtypeCombo->clear();

    const QVariant parentId = m_vehicleTypeCombo->currentData();
    if (!parentId.isValid())
        return;

    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();
    if (!db.isOpen())
        return;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, name FROM vehicle_categories_cat WHERE parent_id = :parent_id ORDER BY name"));
    query.bindValue(QStringLiteral(":parent_id"), parentId);
    if (query.exec()) {
        while (query.next())
            m_subtypeCombo->addItem(query.value(1).toString(), query.value(0));
    }
}

void Step1DetailsView::loadLookups()
{
    ConnectionPool::Handle handle = ConnectionPool::instance().acquire();
    QSqlDatabase &db = handle.database();
    if (!db.isOpen())
        return;

    m_vehicleTypeCombo->clear();
    QSqlQuery typeQuery(db);
    if (typeQuery.exec(QStringLiteral(
            "SELECT id, name FROM vehicle_categories_cat WHERE parent_id IS NULL ORDER BY name"))) {
        while (typeQuery.next())
            m_vehicleTypeCombo->addItem(typeQuery.value(1).toString(), typeQuery.value(0));
    }

    m_brandCombo->clear();
    QSqlQuery brandQuery(db);
    if (brandQuery.exec(QStringLiteral("SELECT id, name FROM brands_cat ORDER BY name"))) {
        while (brandQuery.next())
            m_brandCombo->addItem(brandQuery.value(1).toString(), brandQuery.value(0));
    }

    QSqlQuery umaQuery(db);
    umaQuery.prepare(QStringLiteral("SELECT value_param FROM global_configurations WHERE key_param = :key"));
    umaQuery.bindValue(QStringLiteral(":key"), QStringLiteral("UMA_DIARIA"));
    if (umaQuery.exec() && umaQuery.next())
        m_umaValue = umaQuery.value(0).toDouble();
}

bool Step1DetailsView::validate(QString &errorMessage) const
{
    if (m_serialNumberEdit->text().trimmed().isEmpty()) {
        errorMessage = QStringLiteral("El No. de Serie (VIN) es obligatorio.");
        return false;
    }
    if (m_brandCombo->currentIndex() < 0 || m_brandCombo->currentText().trimmed().isEmpty()) {
        errorMessage = QStringLiteral("Selecciona la Marca del vehículo.");
        return false;
    }
    if (m_modelEdit->text().trimmed().isEmpty()) {
        errorMessage = QStringLiteral("El Modelo es obligatorio.");
        return false;
    }
    if (m_purchasePriceSpin->value() <= 0) {
        errorMessage = QStringLiteral("El Precio de Compra debe ser mayor a cero.");
        return false;
    }
    if (m_ownerNameEdit->text().trimmed().isEmpty()) {
        errorMessage = QStringLiteral("El nombre del Propietario anterior es obligatorio.");
        return false;
    }

    // Regla UMA: un pago de contado no puede alcanzar/superar 3210 UMA.
    if (m_paymentTypeCombo->currentText() == QStringLiteral("Contado")) {
        const double limit = 3210.0 * m_umaValue;
        if (m_purchasePriceSpin->value() >= limit) {
            errorMessage = QStringLiteral(
                "El pago de contado no puede ser mayor o igual a 3210 UMA ($%1). "
                "Cambia el tipo de pago a Crédito o ajusta el precio.")
                .arg(limit, 0, 'f', 2);
            return false;
        }
    }

    return true;
}

void Step1DetailsView::showError(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorLabel->setVisible(true);
}

void Step1DetailsView::hideError()
{
    m_errorLabel->setVisible(false);
}

void Step1DetailsView::fillDraft(VehicleDraft &draft) const
{
    draft.date = m_dateEdit->date();
    draft.vehicleTypeId = m_vehicleTypeCombo->currentData().toString();
    draft.subtypeId = m_subtypeCombo->currentData().toString();
    draft.brandId = m_brandCombo->currentData().toString();
    draft.brandName = m_brandCombo->currentText();
    draft.model = m_modelEdit->text().trimmed();
    draft.yearModel = m_yearModelSpin->value();
    draft.color = m_colorEdit->text().trimmed();
    draft.mileage = m_mileageSpin->value();
    draft.description = m_descriptionEdit->toPlainText();
    draft.motorNumber = m_motorNumberEdit->text().trimmed();
    draft.serialNumber = m_serialNumberEdit->text().trimmed();
    draft.repuve = m_repuveEdit->text().trimmed();
    draft.plates = m_platesEdit->text().trimmed();
    draft.platesHolder = m_platesHolderEdit->text().trimmed();

    draft.owner.fullName = m_ownerNameEdit->text().trimmed();
    draft.owner.nationalId = m_ownerIdEdit->text().trimmed();
    draft.owner.streetAddress = m_ownerAddressEdit->text().trimmed();
    draft.owner.suburb = m_ownerSuburbEdit->text().trimmed();
    draft.owner.locality = m_ownerLocalityEdit->text().trimmed();
    draft.owner.state = m_ownerStateEdit->text().trimmed();
    draft.owner.postalCode = m_ownerPostalCodeEdit->text().trimmed();

    draft.invoiceType = m_invoiceTypeCombo->currentText();
    draft.invoiceFilePath = m_invoiceFilePath;
    draft.invoiceNumber = m_invoiceNumberEdit->text().trimmed();
    draft.invoiceIssuer = m_invoiceIssuerEdit->text().trimmed();

    draft.purchasePrice = m_purchasePriceSpin->value();
    draft.paymentType = m_paymentTypeCombo->currentText();
    draft.paymentMethod = m_paymentMethodCombo->currentText();
    draft.maintenanceCost = m_maintenanceCostSpin->value();
    draft.salePrice = m_salePriceSpin->value();
    draft.observations = m_observationsEdit->toPlainText();
}
