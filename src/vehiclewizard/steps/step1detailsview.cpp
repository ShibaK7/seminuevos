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
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDesktopServices>

namespace {

// Traduce la selección de un combo de catálogo a la referencia del dominio.
// Un combo vacío -- el de subtipo lo queda cada vez que cambia el tipo padre
// -- produce una referencia inválida, que el repositorio guarda como NULL en
// lugar de como id cero, que rompería la llave foránea.
domain::CatalogRef catalogRefFrom(const QComboBox *combo)
{
    domain::CatalogRef ref;
    const QVariant data = combo->currentData();
    if (data.isValid() && !data.isNull())
        ref.id = data.toInt();
    ref.name = combo->currentText();
    return ref;
}

// Texto de la etiqueta de factura cuando no hay archivo. Constante porque se
// escribe en dos sitios -- al armar la tarjeta y al retirar el aviso de
// "factura quitada" -- y deben coincidir.
const QString kNoInvoiceFileText = QStringLiteral("Sin archivo");

} // namespace

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
    connect(m_acquisitionTypeCombo, &QComboBox::currentIndexChanged, this,
            &Step1DetailsView::onAcquisitionTypeChanged);
    connect(m_invoiceTypeCombo, &QComboBox::currentIndexChanged, this,
            &Step1DetailsView::onInvoiceTypeChanged);
    // Deja la vista coherente con la rama seleccionada por omisión.
    onAcquisitionTypeChanged();
    // Llamada explícita aunque el repoblado de arriba ya haya emitido
    // currentIndexChanged: así el estado inicial de los botones de factura no
    // depende de que esa señal se emita ni del orden en que se conectó. Basta
    // con refrescar los botones, sin pasar por onInvoiceTypeChanged(): al
    // construir todavía no hay factura elegida que quitar.
    refreshInvoiceFileControls();
}

domain::AcquisitionType Step1DetailsView::selectedAcquisitionType() const
{
    return static_cast<domain::AcquisitionType>(m_acquisitionTypeCombo->currentData().toInt());
}

bool Step1DetailsView::isAutofacturaSelected() const
{
    // onAcquisitionTypeChanged() vacía el combo antes de repoblarlo, y en ese
    // instante currentData() es un QVariant inválido. Se revisa explícitamente
    // en vez de confiar en que toInt() devuelva 0: hoy 0 es Facturado y el
    // resultado saldría bien por casualidad, pero bastaría reordenar el enum
    // para que un combo vacío contara como autofactura, y entonces el solo
    // hecho de cambiar de rama quitaría la factura ya elegida.
    const QVariant invoiceData = m_invoiceTypeCombo->currentData();
    return invoiceData.isValid()
           && static_cast<domain::InvoiceType>(invoiceData.toInt()) == domain::InvoiceType::Autofactura;
}

void Step1DetailsView::onAcquisitionTypeChanged()
{
    const domain::AcquisitionType type = selectedAcquisitionType();
    const bool isAcquisition = type == domain::AcquisitionType::Adquisicion;

    for (QWidget *widget : std::as_const(m_acquisitionOnlyWidgets))
        widget->setVisible(isAcquisition);
    for (QWidget *widget : std::as_const(m_consignmentOnlyWidgets))
        widget->setVisible(!isAcquisition);

    // Repoblar el combo de factura NO es cosmético. Los CHECK de las dos
    // subtablas admiten conjuntos disjuntos, así que dejarlo con los valores
    // de la otra rama haría que el INSERT violara la restricción -- y el
    // error llegaría desde el hilo de guardado, con los archivos ya copiados
    // a disco.
    const QString previous = m_invoiceTypeCombo->currentText();
    m_invoiceTypeCombo->clear();
    for (domain::InvoiceType value : domain::invoiceTypesFor(type))
        m_invoiceTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    // Si el valor anterior sigue siendo válido en la rama nueva, se conserva.
    const int restored = m_invoiceTypeCombo->findText(previous);
    if (restored >= 0)
        m_invoiceTypeCombo->setCurrentIndex(restored);

    // En una compra la contraparte vende la unidad; en una consignación sigue
    // siendo su dueña.
    m_counterpartyLabel->setText(isAcquisition ? QStringLiteral("Vendedor:")
                                               : QStringLiteral("Propietario:"));
}

void Step1DetailsView::onInvoiceTypeChanged()
{
    discardInvoiceFileIfCfdiRequestPending();
    refreshInvoiceFileControls();
}

void Step1DetailsView::discardInvoiceFileIfCfdiRequestPending()
{
    // Solo se llama desde onInvoiceTypeChanged(), así que si se cumple la
    // condición es porque el tipo ACABA de pasar a Autofactura; mientras siga
    // en él, el botón bloqueado impide elegir otro archivo. Con la solicitud
    // ya generada no se quita nada: cualquier factura adjunta se subió
    // después de generarla, que es justo el orden que se pide.
    if (!isAutofacturaSelected() || m_cfdiRequestGenerated || m_invoiceFilePath.isEmpty())
        return;

    // Se quita, pero no en silencio: la etiqueta, que mostraba el nombre del
    // archivo, pasa a explicar por qué ya no está, para que nadie guarde
    // creyendo que la factura sigue adjunta.
    m_invoiceFilePath.clear();
    m_invoiceFileLabel->setText(QStringLiteral("Factura quitada: primero genera la solicitud de CFDI"));
    m_invoiceRemovedNoticeShown = true;
}

void Step1DetailsView::refreshInvoiceFileControls()
{
    const bool isAutofactura = isAutofacturaSelected();

    // Ocultar el botón por su cuenta no choca con que la fila entera se oculte
    // en la consignación: un hijo ocultado explícitamente sigue oculto cuando
    // su padre se vuelve a mostrar, así que un mecanismo no deshace al otro.
    m_cfdiRequestButton->setVisible(isAutofactura);

    // Con cualquier otro tipo de factura la subida queda libre, como antes.
    // Aquí solo se bloquea el botón; el archivo ya elegido no se toca. Al
    // cambiar de tipo ese archivo se conserva, porque descartarlo en silencio
    // dejaría al usuario guardando sin la factura que cree haber subido. La
    // excepción es pasar a autofactura sin la solicitud: conservarlo ahí
    // equivaldría a saltarse el paso que la autofactura exige, así que
    // discardInvoiceFileIfCfdiRequestPending() lo quita y deja el aviso en la
    // etiqueta.
    const bool uploadLocked = isAutofactura && !m_cfdiRequestGenerated;
    m_invoiceUploadButton->setEnabled(!uploadLocked);
    // Qt muestra el tooltip aun con el botón deshabilitado, así que ahí se
    // explica el bloqueo. Se limpia al desbloquear para no dejar un aviso que
    // ya no aplica.
    m_invoiceUploadButton->setToolTip(uploadLocked
                                          ? QStringLiteral("Primero genera la solicitud de CFDI.")
                                          : QString());

    // El aviso de "factura quitada" solo dice algo cierto mientras la subida
    // siga bloqueada. Si ya se generó la solicitud, o se volvió a un tipo sin
    // CFDI, seguir mostrándolo invitaría a buscar un botón que ya no hace falta
    // (en Facturado ni siquiera se ve). La etiqueta vuelve a su texto neutro;
    // con el aviso a la vista no hay archivo adjunto, así que "Sin archivo" es
    // exacto.
    if (m_invoiceRemovedNoticeShown && !uploadLocked) {
        m_invoiceFileLabel->setText(kNoInvoiceFileText);
        m_invoiceRemovedNoticeShown = false;
    }
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

    // El tipo de operación va primero porque condiciona todo lo demás: qué
    // campos de precio se piden y qué valores admite el combo de factura.
    m_acquisitionTypeCombo = new QComboBox(card);
    m_acquisitionTypeCombo->setObjectName(QStringLiteral("acquisitionTypeCombo"));
    for (domain::AcquisitionType value : domain::allAcquisitionTypes())
        m_acquisitionTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    grid->addWidget(new QLabel(QStringLiteral("Tipo de Operación:"), card), row, 0);
    grid->addWidget(m_acquisitionTypeCombo, row, 1);
    ++row;

    m_ownerNameEdit = new QLineEdit(card);
    m_counterpartyLabel = new QLabel(QStringLiteral("Propietario:"), card);
    grid->addWidget(m_counterpartyLabel, row, 0);
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

    // El combo se llena en onAcquisitionTypeChanged(): los valores válidos
    // dependen de la rama y los conjuntos son disjuntos.
    m_invoiceTypeCombo = new QComboBox(card);
    m_invoiceTypeCombo->setObjectName(QStringLiteral("invoiceTypeCombo"));
    grid->addWidget(new QLabel(QStringLiteral("Tipo Factura:"), card), row, 0);
    grid->addWidget(m_invoiceTypeCombo, row, 1);

    // El archivo de factura solo existe en la compra: el esquema pone
    // invoice_file_path únicamente en vehicle_acquisitions. Va envuelto en un
    // widget porque un QHBoxLayout suelto no se puede ocultar de una pieza.
    auto *invoiceFileWidget = new QWidget(card);
    auto *invoiceFileLayout = new QHBoxLayout(invoiceFileWidget);
    invoiceFileLayout->setContentsMargins(0, 0, 0, 0);
    m_invoiceFileLabel = new QLabel(kNoInvoiceFileText, invoiceFileWidget);

    // Solo aplica a la autofactura, y en ella hay que usarlo antes de poder
    // subir la factura. Ni la visibilidad ni el bloqueo se fijan aquí: los
    // aplica refreshInvoiceFileControls() cada vez que cambia el tipo de
    // factura o se genera la solicitud.
    m_cfdiRequestButton = new QPushButton(QStringLiteral("Generar solicitud CFDI"), invoiceFileWidget);
    m_cfdiRequestButton->setObjectName(QStringLiteral("cfdiRequestButton"));
    m_cfdiRequestButton->setProperty("class", QStringLiteral("secondary"));
    connect(m_cfdiRequestButton, &QPushButton::clicked, this, &Step1DetailsView::generateCfdiRequest);

    m_invoiceUploadButton = new QPushButton(QStringLiteral("Subir documento"), invoiceFileWidget);
    m_invoiceUploadButton->setObjectName(QStringLiteral("invoiceUploadButton"));
    m_invoiceUploadButton->setProperty("class", QStringLiteral("secondary"));
    connect(m_invoiceUploadButton, &QPushButton::clicked, this, &Step1DetailsView::onBrowseInvoiceFile);

    // Cada widget se agrega una sola vez: un segundo addWidget() con el mismo
    // widget no lo duplica, lo MUEVE (Qt lo saca de su lugar anterior), y así
    // la etiqueta acababa entre los dos botones. El de CFDI va antes que el de
    // subir porque en autofactura hay que usarlo primero: leída de izquierda a
    // derecha, la fila repite la secuencia que se exige.
    invoiceFileLayout->addWidget(m_invoiceFileLabel, 1);
    invoiceFileLayout->addWidget(m_cfdiRequestButton);
    invoiceFileLayout->addWidget(m_invoiceUploadButton);

    auto *invoiceFileRowLabel = new QLabel(QStringLiteral("Factura:"), card);
    grid->addWidget(invoiceFileRowLabel, row, 2);
    grid->addWidget(invoiceFileWidget, row, 3);
    m_acquisitionOnlyWidgets << invoiceFileRowLabel << invoiceFileWidget;
    ++row;

    m_invoiceNumberEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("No. Factura:"), card), row, 0);
    grid->addWidget(m_invoiceNumberEdit, row, 1);

    m_invoiceIssuerEdit = new QLineEdit(card);
    grid->addWidget(new QLabel(QStringLiteral("Expidió Factura:"), card), row, 2);
    grid->addWidget(m_invoiceIssuerEdit, row, 3);
    ++row;

    // Los campos propios de cada rama ocupan FILAS COMPLETAS, no medias
    // filas compartidas con campos comunes: así, al ocultar una rama, sus
    // filas colapsan enteras en vez de dejar huecos a un lado.

    // --- Solo Adquisición ---
    m_purchasePriceSpin = new QDoubleSpinBox(card);
    m_purchasePriceSpin->setRange(0, 99999999);
    m_purchasePriceSpin->setPrefix(QStringLiteral("$ "));
    m_purchasePriceSpin->setDecimals(2);
    auto *purchasePriceLabel = new QLabel(QStringLiteral("Precio Compra:"), card);
    grid->addWidget(purchasePriceLabel, row, 0);
    grid->addWidget(m_purchasePriceSpin, row, 1);

    m_paymentTypeCombo = new QComboBox(card);
    for (domain::PaymentType value : domain::allPaymentTypes())
        m_paymentTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    auto *paymentTypeLabel = new QLabel(QStringLiteral("Tipo Pago:"), card);
    grid->addWidget(paymentTypeLabel, row, 2);
    grid->addWidget(m_paymentTypeCombo, row, 3);
    m_acquisitionOnlyWidgets << purchasePriceLabel << m_purchasePriceSpin
                             << paymentTypeLabel << m_paymentTypeCombo;
    ++row;

    m_paymentMethodCombo = new QComboBox(card);
    for (domain::PaymentMethod value : domain::allPaymentMethods())
        m_paymentMethodCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    auto *paymentMethodLabel = new QLabel(QStringLiteral("Método de Pago:"), card);
    grid->addWidget(paymentMethodLabel, row, 0);
    grid->addWidget(m_paymentMethodCombo, row, 1);

    m_salePriceSpin = new QDoubleSpinBox(card);
    m_salePriceSpin->setRange(0, 99999999);
    m_salePriceSpin->setPrefix(QStringLiteral("$ "));
    auto *salePriceLabel = new QLabel(QStringLiteral("Precio Venta:"), card);
    grid->addWidget(salePriceLabel, row, 2);
    grid->addWidget(m_salePriceSpin, row, 3);
    m_acquisitionOnlyWidgets << paymentMethodLabel << m_paymentMethodCombo
                             << salePriceLabel << m_salePriceSpin;
    ++row;

    // --- Solo Consignación ---
    // No hay precio de venta que capturar: sale de base + comisión, igual que
    // la columna generada de vehicle_consignments.
    m_basePriceSpin = new QDoubleSpinBox(card);
    m_basePriceSpin->setRange(0, 99999999);
    m_basePriceSpin->setPrefix(QStringLiteral("$ "));
    m_basePriceSpin->setDecimals(2);
    auto *basePriceLabel = new QLabel(QStringLiteral("Precio Base (dueño):"), card);
    grid->addWidget(basePriceLabel, row, 0);
    grid->addWidget(m_basePriceSpin, row, 1);

    m_commissionRateSpin = new QDoubleSpinBox(card);
    m_commissionRateSpin->setRange(0, 100);
    m_commissionRateSpin->setSuffix(QStringLiteral(" %"));
    m_commissionRateSpin->setDecimals(2);
    auto *commissionRateLabel = new QLabel(QStringLiteral("Comisión:"), card);
    grid->addWidget(commissionRateLabel, row, 2);
    grid->addWidget(m_commissionRateSpin, row, 3);
    m_consignmentOnlyWidgets << basePriceLabel << m_basePriceSpin
                             << commissionRateLabel << m_commissionRateSpin;
    ++row;

    // --- Común a las dos ramas ---
    m_maintenanceCostSpin = new QDoubleSpinBox(card);
    m_maintenanceCostSpin->setRange(0, 9999999);
    m_maintenanceCostSpin->setPrefix(QStringLiteral("$ "));
    grid->addWidget(new QLabel(QStringLiteral("Mantenimientos:"), card), row, 0);
    grid->addWidget(m_maintenanceCostSpin, row, 1);
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
    // El nombre del archivo reemplaza al aviso que pudiera haber.
    m_invoiceRemovedNoticeShown = false;
}

void Step1DetailsView::generateCfdiRequest() {
    constexpr const char *kCfdiTemplateResourcePath =
        ":/templates/request_issuance_cfdi.html";
    QFile resourceFile(kCfdiTemplateResourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("No se pudo abrir el documento"),
            tr("No se encontró el recurso:\n%1")
                .arg(kCfdiTemplateResourcePath));
        return;
    }

    const QByteArray htmlContent = resourceFile.readAll();
    resourceFile.close();

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString tempFilePath = QDir(tempDir).filePath("request_issuance_cfdi.html");

    QFile tempFile(tempFilePath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("No se pudo abrir el documento"),
                            tr("No se pudo crear el archivo temporal en:\n%1")
                                .arg(tempFilePath));
        return;
    }

    // Que el temporal quede completo es condición para seguir: el éxito
    // desbloquea la subida, y un temporal truncado la desbloquearía con una
    // solicitud a medias. En modo Text, write() cuenta los bytes de
    // htmlContent y no el \r que Windows agrega a cada salto, así que se
    // compara contra size() tal cual. Y flush() se llama a mano porque una
    // plantilla de pocos KB se queda entera en el búfer de QFile: un error de
    // disco no aparecería hasta vaciarlo, y close() no lo reporta.
    const bool fullyWritten =
        tempFile.write(htmlContent) == htmlContent.size() && tempFile.flush();
    tempFile.close();
    if (!fullyWritten) {
        QMessageBox::warning(this, tr("No se pudo abrir el documento"),
                            tr("No se pudo escribir por completo el archivo temporal en:\n%1")
                                .arg(tempFilePath));
        return;
    }

    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(tempFilePath));
    if (!opened) {
        QMessageBox::warning(this, tr("No se pudo abrir el documento"),
                            tr("No se pudo abrir el navegador para mostrar:\n%1")
                                .arg(tempFilePath));
        return;
    }

    // Solo cuenta como generada cuando todo salió bien: se leyó la plantilla,
    // el temporal se creó y se escribió completo, y openUrl() lo abrió. Cada
    // falla anterior sale por su propio return sin tocar la bandera, para no
    // desbloquear la subida con una solicitud que el usuario nunca llegó a ver.
    m_cfdiRequestGenerated = true;
    refreshInvoiceFileControls();
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

domain::ValidationResult Step1DetailsView::validate() const
{
    // Se arma un builder desechable con lo capturado y se le pregunta al
    // dominio. Las reglas (VIN obligatorio, precio mayor a cero, el tope de
    // las 3210 UMA para pagos en efectivo) viven en AcquiredVehicle, no aquí.
    domain::VehicleBuilder builder;
    applyTo(builder);
    return builder.validateVehicleData();
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

void Step1DetailsView::applyTo(domain::VehicleBuilder &builder) const
{
    // El tipo de operación va primero: decide qué subclase construye el
    // builder, y todo lo demás se aplica sobre ella.
    builder.setAcquisitionType(selectedAcquisitionType());

    builder.setDealDate(m_dateEdit->date())
        .setVehicleType(catalogRefFrom(m_vehicleTypeCombo))
        .setSubtype(catalogRefFrom(m_subtypeCombo))
        .setBrand(catalogRefFrom(m_brandCombo))
        .setModel(m_modelEdit->text())
        .setYearModel(m_yearModelSpin->value())
        .setColor(m_colorEdit->text())
        .setMileage(m_mileageSpin->value())
        .setDescription(m_descriptionEdit->toPlainText())
        .setMotorNumber(m_motorNumberEdit->text())
        .setSerialNumber(m_serialNumberEdit->text())
        .setRepuve(m_repuveEdit->text())
        .setPlates(m_platesEdit->text())
        .setPlatesHolder(m_platesHolderEdit->text());

    domain::Counterparty owner;
    // Los setters de Counterparty devuelven bool, pero aquí los valores vienen
    // de campos de texto acotados por la interfaz. Lo que sí puede fallar de
    // verdad -- código postal o teléfono mal formados -- se refleja en que el
    // campo queda vacío, y el contrato lo reporta como domicilio incompleto.
    (void)owner.setFullName(m_ownerNameEdit->text());
    (void)owner.setNationalId(m_ownerIdEdit->text());
    owner.setStreetAddress(m_ownerAddressEdit->text());
    (void)owner.setSuburb(m_ownerSuburbEdit->text());
    (void)owner.setLocality(m_ownerLocalityEdit->text());
    (void)owner.setState(m_ownerStateEdit->text());
    (void)owner.setPostalCode(m_ownerPostalCodeEdit->text());
    builder.setCounterparty(owner);

    // El userData del combo guarda el enum, así que no hay que traducir el
    // texto de vuelta ni depender de cómo esté escrita la etiqueta.
    builder.setInvoiceType(
        static_cast<domain::InvoiceType>(m_invoiceTypeCombo->currentData().toInt()));
    builder.setInvoiceNumber(m_invoiceNumberEdit->text())
        .setInvoiceIssuer(m_invoiceIssuerEdit->text())
        .setMaintenanceCost(m_maintenanceCostSpin->value())
        .setObservations(m_observationsEdit->toPlainText());

    // Los datos propios de cada rama se mandan siempre: el builder ignora los
    // que no corresponden a la subclase que construyó, así que no hace falta
    // ramificar aquí también.
    builder.setInvoiceFilePath(m_invoiceFilePath)
        .setSalePrice(m_salePriceSpin->value())
        .setPaymentType(static_cast<domain::PaymentType>(m_paymentTypeCombo->currentData().toInt()))
        .setPaymentMethod(
            static_cast<domain::PaymentMethod>(m_paymentMethodCombo->currentData().toInt()))
        // La UMA se inyecta desde aquí porque el dominio no consulta la base:
        // la regla del pago en efectivo necesita el valor vigente, y quien lo
        // leyó de global_configurations fue loadLookups().
        .setUmaDailyValue(m_umaValue)
        .setCommissionRate(m_commissionRateSpin->value());

    // Los precios en cero no se mandan: el rango del control impide valores
    // negativos, así que un cero solo significa "sin capturar". Mandarlo haría
    // que el setter lo rechazara y se reportara dos veces el mismo problema,
    // una con el mensaje del rechazo y otra con el de la validación, que es
    // el que de verdad describe lo que falta.
    if (m_purchasePriceSpin->value() > 0.0)
        builder.setPurchasePrice(m_purchasePriceSpin->value());
    if (m_basePriceSpin->value() > 0.0)
        builder.setBasePrice(m_basePriceSpin->value());
}
