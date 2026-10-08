#include "presentation/views/wizard/vehicledetailsview.h"
#include "presentation/views/support/formsupport.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
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
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QCompleter>
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
// escribe en dos sitios -- al armar la tarjeta y al descartar la factura
// apartada cuando se genera la solicitud de CFDI -- y deben coincidir.
const QString kNoInvoiceFileText = QStringLiteral("Sin archivo");

} // namespace

VehicleDetailsView::VehicleDetailsView(QWidget *parent)
    : QWidget(parent)
{
    // Sin aviso propio: los errores de este paso los explica el aviso del
    // asistente, y los campos que fallan se marcan por su propiedad "field".
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(buildGeneralInfoCard());
    layout->addWidget(buildOwnerAndAcquisitionCard());

    connect(m_vehicleTypeCombo, &QComboBox::currentIndexChanged, this, &VehicleDetailsView::reloadSubtypes);
    connect(m_acquisitionTypeCombo, &QComboBox::currentIndexChanged, this,
            &VehicleDetailsView::onAcquisitionTypeChanged);
    connect(m_invoiceTypeCombo, &QComboBox::currentIndexChanged, this,
            &VehicleDetailsView::onInvoiceTypeChanged);
    // Deja la vista coherente con la rama seleccionada por omisión.
    onAcquisitionTypeChanged();
    // Llamada explícita aunque el repoblado de arriba ya haya emitido
    // currentIndexChanged: así el estado inicial de los botones de factura no
    // depende de que esa señal se emita ni del orden en que se conectó. Basta
    // con refrescar los botones, sin pasar por onInvoiceTypeChanged(): al
    // construir todavía no hay factura que apartar ni una apartada que
    // recuperar.
    refreshInvoiceFileControls();
}

domain::AcquisitionType VehicleDetailsView::selectedAcquisitionType() const
{
    return static_cast<domain::AcquisitionType>(m_acquisitionTypeCombo->currentData().toInt());
}

bool VehicleDetailsView::isAutofacturaSelected() const
{
    // onAcquisitionTypeChanged() vacía el combo antes de repoblarlo, y en ese
    // instante currentData() es un QVariant inválido. Se revisa explícitamente
    // en vez de confiar en que toInt() devuelva 0: hoy 0 es Facturado y el
    // resultado saldría bien por casualidad, pero bastaría reordenar el enum
    // para que un combo vacío contara como autofactura, y entonces el solo
    // hecho de cambiar de rama apartaría la factura ya elegida, y recuperarla
    // quedaría en manos de la señal siguiente.
    const QVariant invoiceData = m_invoiceTypeCombo->currentData();
    return invoiceData.isValid()
           && static_cast<domain::InvoiceType>(invoiceData.toInt()) == domain::InvoiceType::Autofactura;
}

void VehicleDetailsView::onAcquisitionTypeChanged()
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
    m_counterpartyLabel->setText(isAcquisition ? QStringLiteral("Vendedor: <span style='color: #D90429; font-weight: bold;'>*</span>")
                                               : QStringLiteral("Propietario: <span style='color: #D90429; font-weight: bold;'>*</span>"));
}

void VehicleDetailsView::onInvoiceTypeChanged()
{
    // Sin solicitud, entrar a Autofactura aparta la factura y salir de ella la
    // devuelve. El índice -1 que deja onAcquisitionTypeChanged() mientras
    // vacía el combo cuenta como salir, así que cambiar de rama también la
    // recupera: al volver a Adquisición, ya en Facturado, sigue adjunta, igual
    // que cualquier factura sobrevive a ir y volver por la consignación.
    if (isAutofacturaSelected())
        suspendInvoiceFileIfCfdiRequestPending();
    else
        restoreSuspendedInvoiceFile();
    refreshInvoiceFileControls();
}

void VehicleDetailsView::suspendInvoiceFileIfCfdiRequestPending()
{
    // El tipo se revisa aquí aunque onInvoiceTypeChanged() solo llama a esta
    // función con Autofactura elegida, para que sea segura desde cualquier
    // sitio: con otro tipo no aparta nada, igual que
    // restoreSuspendedInvoiceFile() no recupera nada con Autofactura elegida.
    // Con Autofactura y sin solicitud, si hay archivo es porque el tipo ACABA
    // de pasar a Autofactura; mientras siga en él, el botón bloqueado impide
    // elegir otro. Con la solicitud ya generada no se aparta nada: cualquier
    // factura adjunta se subió después de generarla, que es justo el orden
    // que se pide.
    if (!isAutofacturaSelected() || m_cfdiRequestGenerated || m_invoiceFilePath.isEmpty())
        return;

    // Se aparta, pero no en silencio: la etiqueta, que mostraba el nombre del
    // archivo, pasa a decir que ya no está adjunto, para que nadie guarde
    // creyendo que la factura sigue ahí. El aviso es corto a propósito: la
    // etiqueta no parte el texto en renglones y comparte columna con los dos
    // botones, así que uno largo ensancharía todo el paso y se cortaría en una
    // ventana de tamaño normal. El porqué, y cómo recuperarla, van en el
    // tooltip.
    m_suspendedInvoiceFilePath = m_invoiceFilePath;
    m_invoiceFilePath.clear();
    m_invoiceFileLabel->setText(QStringLiteral("Factura retirada"));
    m_invoiceFileLabel->setToolTip(QStringLiteral(
        "La autofactura pide generar primero la solicitud de CFDI y subir la factura después. "
        "Si regresas a Facturado, se recupera la que habías elegido."));
}

void VehicleDetailsView::restoreSuspendedInvoiceFile()
{
    // También revisa el tipo por su cuenta, igual que
    // suspendInvoiceFileIfCfdiRequestPending(): con Autofactura elegida,
    // recuperarla volvería a adjuntar una factura elegida antes de la
    // solicitud, justo lo que apartarla evita. El índice -1 que deja
    // onAcquisitionTypeChanged() al vaciar el combo no cuenta como
    // Autofactura, así que cambiar de rama la sigue recuperando.
    if (isAutofacturaSelected() || m_suspendedInvoiceFilePath.isEmpty())
        return;

    // Vuelve tal como estaba antes de apartarse: adjunta, con la etiqueta
    // diciendo lo mismo que puso onBrowseInvoiceFile() al elegirla, y sin el
    // tooltip, que solo explicaba por qué no estaba.
    m_invoiceFilePath = m_suspendedInvoiceFilePath;
    m_suspendedInvoiceFilePath.clear();
    m_invoiceFileLabel->setText(QFileInfo(m_invoiceFilePath).fileName());
    m_invoiceFileLabel->setToolTip(QString());
}

void VehicleDetailsView::refreshInvoiceFileControls()
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
    // excepción es pasar a autofactura sin la solicitud: dejarlo adjunto ahí
    // equivaldría a saltarse el paso que la autofactura exige, así que
    // suspendInvoiceFileIfCfdiRequestPending() lo aparta, sin tirarlo, y la
    // etiqueta lo avisa.
    const bool uploadLocked = isAutofactura && !m_cfdiRequestGenerated;
    m_invoiceUploadButton->setEnabled(!uploadLocked);
    // Qt muestra el tooltip aun con el botón deshabilitado, así que ahí se
    // explica el bloqueo. Se limpia al desbloquear para no dejar un aviso que
    // ya no aplica.
    m_invoiceUploadButton->setToolTip(uploadLocked
                                          ? QStringLiteral("Primero genera la solicitud de CFDI.")
                                          : QString());
}

QWidget *VehicleDetailsView::buildGeneralInfoCard()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    formsupport::applyFloatingShadow(card);

    auto *cardLayout = new QVBoxLayout(card);
    // Folio es la primera fila del formulario -- que quede pegada arriba
    // del contenedor, con solo unos pocos px de aire, no el margen grande
    // por defecto del layout.
    cardLayout->setContentsMargins(16, 16, 16, 16);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(25);
    grid->setVerticalSpacing(8);

    int row = 0;

    // Cada widget de captura lleva en "field" la clave con la que el dominio
    // reporta sus errores (la misma que usan Vehicle::validate() y los
    // rechazos de VehicleBuilder). Con ella el asistente marca en rojo los que
    // fallan y le da el foco al primero, sin que esta vista traduzca errores.
    // También la llevan los campos que hoy no tienen regla: si el dominio
    // agrega una, la marca funciona sin tocar esta vista.
    m_folioEdit = new QLineEdit(card);
    m_folioEdit->setProperty("field", QStringLiteral("folio"));
    m_folioEdit->setPlaceholderText(QStringLiteral("(auto)"));
    m_folioEdit->setDisabled(true);
    m_dateEdit = new QDateEdit(QDate::currentDate(), card);
    m_dateEdit->setProperty("field", QStringLiteral("dealDate"));
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setMaximumDate(QDate::currentDate());
    QDate minimumDate(2000, 1, 1);
    m_dateEdit->setMinimumDate(minimumDate);
    m_dateEdit->setMaximumDate(QDate::currentDate());
    m_vehicleTypeCombo = new QComboBox(card);
    m_vehicleTypeCombo->setProperty("field", QStringLiteral("vehicleType"));
    m_subtypeCombo = new QComboBox(card);
    m_subtypeCombo->setProperty("field", QStringLiteral("subtype"));
    m_brandCombo = new QComboBox(card);
    m_brandCombo->setProperty("field", QStringLiteral("brand"));
    m_modelEdit = new QLineEdit(card);
    m_modelEdit->setProperty("field", QStringLiteral("model"));
    m_modelEdit->setPlaceholderText(QStringLiteral("p.ej. Kicks Advance TM"));
    m_yearModelSpin = new QSpinBox(card);
    m_yearModelSpin->setProperty("field", QStringLiteral("yearModel"));
    m_yearModelSpin->setRange(1980, QDate::currentDate().year() + 1);
    m_yearModelSpin->setValue(QDate::currentDate().year());


    // Sin asterisco: el folio lo asigna la base al guardar, el usuario no lo
    // captura.
    grid->addWidget(new QLabel(QStringLiteral("Folio:"), card), row, 0);
    grid->addWidget(m_folioEdit, row, 1);
    grid->addWidget(formsupport::requiredLabel("Fecha: ", card), row, 2);
    grid->addWidget(m_dateEdit, row, 3);
    ++row;

    grid->addWidget(formsupport::requiredLabel("Tipo Vehículo: ", card), row, 0);
    grid->addWidget(m_vehicleTypeCombo, row, 1);
    grid->addWidget(formsupport::requiredLabel("Subtipo: ", card), row, 2);
    grid->addWidget(m_subtypeCombo, row, 3);
    ++row;

    grid->addWidget(formsupport::requiredLabel("Marca: ", card), row, 0);
    grid->addWidget(m_brandCombo, row, 1);
    grid->addWidget(formsupport::requiredLabel("Modelo: ", card), row, 2);
    grid->addWidget(m_modelEdit, row, 3);
    ++row;

    grid->addWidget(formsupport::requiredLabel("Año Modelo: ", card), row, 0);
    grid->addWidget(m_yearModelSpin, row, 1);

    m_colorEdit = new QLineEdit(card);
    m_colorEdit->setProperty("field", QStringLiteral("color"));
    grid->addWidget(new QLabel(QStringLiteral("Color:"), card), row, 2);
    grid->addWidget(m_colorEdit, row, 3);
    ++row;

    m_mileageSpin = new QSpinBox(card);
    m_mileageSpin->setProperty("field", QStringLiteral("mileage"));
    m_mileageSpin->setRange(0, 2000000);
    m_mileageSpin->setSuffix(QStringLiteral(" km"));
    grid->addWidget(new QLabel(QStringLiteral("Kilometraje:"), card), row, 0);
    grid->addWidget(m_mileageSpin, row, 1);

    m_motorNumberEdit = new QLineEdit(card);
    m_motorNumberEdit->setProperty("field", QStringLiteral("motorNumber"));
    grid->addWidget(formsupport::requiredLabel("No. Motor: ", card), row, 2);
    grid->addWidget(m_motorNumberEdit, row, 3);
    ++row;

    m_serialNumberEdit = new QLineEdit(card);
    m_serialNumberEdit->setProperty("field", QStringLiteral("serialNumber"));
    m_serialNumberEdit->setPlaceholderText(QStringLiteral("VIN"));
    grid->addWidget(formsupport::requiredLabel("No. Serie (VIN): ", card), row, 0);
    grid->addWidget(m_serialNumberEdit, row, 1);

    m_repuveEdit = new QLineEdit(card);
    m_repuveEdit->setProperty("field", QStringLiteral("repuve"));
    grid->addWidget(formsupport::requiredLabel("REPUVE: ", card), row, 2);
    grid->addWidget(m_repuveEdit, row, 3);
    ++row;

    m_platesEdit = new QLineEdit(card);
    m_platesEdit->setProperty("field", QStringLiteral("plates"));
    grid->addWidget(formsupport::requiredLabel("Placas: ", card), row, 0);
    grid->addWidget(m_platesEdit, row, 1);

    m_platesHolderEdit = new QLineEdit(card);
    m_platesHolderEdit->setProperty("field", QStringLiteral("platesHolder"));
    grid->addWidget(formsupport::requiredLabel("Titular Placas: ", card), row, 2);
    grid->addWidget(m_platesHolderEdit, row, 3);
    ++row;

    m_descriptionEdit = new QTextEdit(card);
    m_descriptionEdit->setProperty("field", QStringLiteral("description"));
    m_descriptionEdit->setMaximumHeight(60);
    grid->addWidget(new QLabel(QStringLiteral("Descripción:"), card), row, 0);
    grid->addWidget(m_descriptionEdit, row, 1, 1, 3);

    cardLayout->addLayout(grid);
    return card;
}

QWidget *VehicleDetailsView::buildOwnerAndAcquisitionCard()
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("cardPanel"));
    formsupport::applyFloatingShadow(card);

    auto *cardLayout = new QVBoxLayout(card);

    cardLayout->setContentsMargins(16, 16, 16, 16);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(25);
    grid->setVerticalSpacing(8);

    int row = 0;

    // El tipo de operación va primero porque condiciona todo lo demás: qué
    // campos de precio se piden y qué valores admite el combo de factura.
    m_acquisitionTypeCombo = new QComboBox(card);
    m_acquisitionTypeCombo->setObjectName(QStringLiteral("acquisitionTypeCombo"));
    m_acquisitionTypeCombo->setProperty("field", QStringLiteral("acquisitionType"));
    for (domain::AcquisitionType value : domain::allAcquisitionTypes())
        m_acquisitionTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    grid->addWidget(formsupport::requiredLabel("Tipo Operación: ", card), row, 0);
    grid->addWidget(m_acquisitionTypeCombo, row, 1);
    ++row;

    // Los datos de la contraparte llevan el prefijo "counterparty.": así los
    // reporta Vehicle::validate() al juntar la validación de Counterparty.
    m_ownerNameEdit = new QLineEdit(card);
    m_ownerNameEdit->setProperty("field", QStringLiteral("counterparty.fullName"));
    m_counterpartyLabel = new QLabel(QStringLiteral("Propietario: <span style='color: #D90429; font-weight: bold;'>*</span>"), card);
    grid->addWidget(m_counterpartyLabel, row, 0);
    grid->addWidget(m_ownerNameEdit, row, 1);

    m_ownerIdEdit = new QLineEdit(card);
    m_ownerIdEdit->setProperty("field", QStringLiteral("counterparty.nationalId"));
    grid->addWidget(formsupport::requiredLabel("Identificación: ", card), row, 2);
    grid->addWidget(m_ownerIdEdit, row, 3);
    ++row;

    m_ownerAddressEdit = new QLineEdit(card);
    m_ownerAddressEdit->setProperty("field", QStringLiteral("counterparty.streetAddress"));
    grid->addWidget(new QLabel(QStringLiteral("Domicilio:"), card), row, 0);
    grid->addWidget(m_ownerAddressEdit, row, 1);

    m_ownerSuburbEdit = new QLineEdit(card);
    m_ownerSuburbEdit->setProperty("field", QStringLiteral("counterparty.suburb"));
    grid->addWidget(new QLabel(QStringLiteral("Colonia:"), card), row, 2);
    grid->addWidget(m_ownerSuburbEdit, row, 3);
    ++row;

    m_ownerLocalityEdit = new QLineEdit(card);
    m_ownerLocalityEdit->setProperty("field", QStringLiteral("counterparty.locality"));
    grid->addWidget(new QLabel(QStringLiteral("Localidad:"), card), row, 0);
    grid->addWidget(m_ownerLocalityEdit, row, 1);

    m_ownerStateEdit = new QLineEdit(card);
    m_ownerStateEdit->setProperty("field", QStringLiteral("counterparty.state"));
    grid->addWidget(new QLabel(QStringLiteral("Estado:"), card), row, 2);
    grid->addWidget(m_ownerStateEdit, row, 3);
    ++row;

    m_ownerPostalCodeEdit = new QLineEdit(card);
    m_ownerPostalCodeEdit->setProperty("field", QStringLiteral("counterparty.postalCode"));
    grid->addWidget(new QLabel(QStringLiteral("Código Postal:"), card), row, 0);
    grid->addWidget(m_ownerPostalCodeEdit, row, 1);
    ++row;

    // El combo se llena en onAcquisitionTypeChanged(): los valores válidos
    // dependen de la rama y los conjuntos son disjuntos.
    m_invoiceTypeCombo = new QComboBox(card);
    m_invoiceTypeCombo->setObjectName(QStringLiteral("invoiceTypeCombo"));
    m_invoiceTypeCombo->setProperty("field", QStringLiteral("invoiceType"));
    grid->addWidget(formsupport::requiredLabel("Tipo Factura: ", card), row, 0);
    grid->addWidget(m_invoiceTypeCombo, row, 1);

    // El archivo de factura solo existe en la compra: el esquema pone
    // invoice_file_path únicamente en vehicle_acquisitions. Va envuelto en un
    // widget porque un QHBoxLayout suelto no se puede ocultar de una pieza.
    // Sus botones no llevan "field": abren un diálogo en vez de capturar un
    // dato, y el tooltip del de subir explica cuándo está bloqueado, algo que
    // el tooltip de un error taparía.
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
    connect(m_cfdiRequestButton, &QPushButton::clicked, this, &VehicleDetailsView::generateCfdiRequest);

    m_invoiceUploadButton = new QPushButton(QStringLiteral("Subir documento"), invoiceFileWidget);
    m_invoiceUploadButton->setObjectName(QStringLiteral("invoiceUploadButton"));
    m_invoiceUploadButton->setProperty("class", QStringLiteral("secondary"));
    connect(m_invoiceUploadButton, &QPushButton::clicked, this, &VehicleDetailsView::onBrowseInvoiceFile);

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
    m_invoiceNumberEdit->setProperty("field", QStringLiteral("invoiceNumber"));
    grid->addWidget(formsupport::requiredLabel("No. Factura: ", card), row, 0);
    grid->addWidget(m_invoiceNumberEdit, row, 1);

    m_invoiceIssuerEdit = new QLineEdit(card);
    m_invoiceIssuerEdit->setProperty("field", QStringLiteral("invoiceIssuer"));
    grid->addWidget(new QLabel(QStringLiteral("Expidió Factura:"), card), row, 2);
    grid->addWidget(m_invoiceIssuerEdit, row, 3);
    ++row;

    // Los campos propios de cada rama ocupan FILAS COMPLETAS, no medias
    // filas compartidas con campos comunes: así, al ocultar una rama, sus
    // filas colapsan enteras en vez de dejar huecos a un lado.

    // --- Solo Adquisición ---
    // Las reglas de los precios y del tope de pago en efectivo no van aquí:
    // son del dominio (AcquiredVehicle), que conoce la UMA vigente y reporta
    // cada error con la clave del campo que lo provoca, y el asistente marca
    // ese campo. Una revisión propia en esta vista repetía el tope con otro
    // criterio, se pintaba en rojo desde que se abría el paso y movía el foco
    // por su cuenta.
    m_purchasePriceSpin = new QDoubleSpinBox(card);
    m_purchasePriceSpin->setProperty("field", QStringLiteral("purchasePrice"));
    m_purchasePriceSpin->setRange(0, 99999999);
    m_purchasePriceSpin->setPrefix(QStringLiteral("$ "));
    m_purchasePriceSpin->setDecimals(2);
    auto *purchasePriceLabel = formsupport::requiredLabel("Precio Compra: ", card);
    grid->addWidget(purchasePriceLabel, row, 0);
    grid->addWidget(m_purchasePriceSpin, row, 1);

    m_paymentTypeCombo = new QComboBox(card);
    m_paymentTypeCombo->setProperty("field", QStringLiteral("paymentType"));
    for (domain::PaymentType value : domain::allPaymentTypes())
        m_paymentTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    auto *paymentTypeLabel = formsupport::requiredLabel("Tipo Pago: ", card);
    grid->addWidget(paymentTypeLabel, row, 2);
    grid->addWidget(m_paymentTypeCombo, row, 3);
    m_acquisitionOnlyWidgets << purchasePriceLabel << m_purchasePriceSpin
                             << paymentTypeLabel << m_paymentTypeCombo;
    ++row;

    m_paymentMethodCombo = new QComboBox(card);
    m_paymentMethodCombo->setProperty("field", QStringLiteral("paymentMethod"));
    for (domain::PaymentMethod value : domain::allPaymentMethods())
        m_paymentMethodCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    auto *paymentMethodLabel = formsupport::requiredLabel("Método Pago: ", card);
    grid->addWidget(paymentMethodLabel, row, 0);
    grid->addWidget(m_paymentMethodCombo, row, 1);

    m_salePriceSpin = new QDoubleSpinBox(card);
    m_salePriceSpin->setProperty("field", QStringLiteral("salePrice"));
    m_salePriceSpin->setRange(0, 99999999);
    m_salePriceSpin->setPrefix(QStringLiteral("$ "));
    auto *salePriceLabel = formsupport::requiredLabel("Precio Venta: ", card);
    grid->addWidget(salePriceLabel, row, 2);
    grid->addWidget(m_salePriceSpin, row, 3);
    m_acquisitionOnlyWidgets << paymentMethodLabel << m_paymentMethodCombo
                             << salePriceLabel << m_salePriceSpin;
    ++row;

    // --- Solo Consignación ---
    // No hay precio de venta que capturar: sale de base + comisión, igual que
    // la columna generada de vehicle_consignments.
    m_basePriceSpin = new QDoubleSpinBox(card);
    m_basePriceSpin->setProperty("field", QStringLiteral("basePrice"));
    m_basePriceSpin->setRange(0, 99999999);
    m_basePriceSpin->setPrefix(QStringLiteral("$ "));
    m_basePriceSpin->setDecimals(2);
    // Obligatorio en consignación: es lo que se le entrega al propietario y lo
    // que imprime el contrato.
    auto *basePriceLabel = formsupport::requiredLabel("Precio Base (dueño): ", card);
    grid->addWidget(basePriceLabel, row, 0);
    grid->addWidget(m_basePriceSpin, row, 1);

    m_commissionRateSpin = new QDoubleSpinBox(card);
    m_commissionRateSpin->setProperty("field", QStringLiteral("commissionRate"));
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
    m_maintenanceCostSpin->setProperty("field", QStringLiteral("maintenanceCost"));
    m_maintenanceCostSpin->setRange(0, 9999999);
    m_maintenanceCostSpin->setPrefix(QStringLiteral("$ "));
    grid->addWidget(new QLabel(QStringLiteral("Mantenimientos:"), card), row, 0);
    grid->addWidget(m_maintenanceCostSpin, row, 1);
    ++row;

    m_observationsEdit = new QTextEdit(card);
    m_observationsEdit->setProperty("field", QStringLiteral("observations"));
    m_observationsEdit->setMaximumHeight(60);
    grid->addWidget(new QLabel(QStringLiteral("Observaciones:"), card), row, 0);
    grid->addWidget(m_observationsEdit, row, 1, 1, 3);

    cardLayout->addLayout(grid);
    return card;
}

void VehicleDetailsView::onBrowseInvoiceFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Seleccionar factura"));
    if (path.isEmpty())
        return;

    m_invoiceFilePath = path;
    m_invoiceFileLabel->setText(QFileInfo(path).fileName());
    // El archivo nuevo deja sin efecto cualquier factura apartada, y su nombre
    // sustituye al aviso, así que el tooltip que lo explicaba sobra. Hoy no
    // puede haber una apartada al elegir archivo -- mientras la hay, la
    // subida está bloqueada --, pero si eso cambiara, salir de Autofactura no
    // debe reemplazar el archivo nuevo por el viejo.
    m_suspendedInvoiceFilePath.clear();
    m_invoiceFileLabel->setToolTip(QString());
}

void VehicleDetailsView::generateCfdiRequest() {
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
    // falla anterior sale por su propio return sin tocar la bandera ni la
    // factura apartada: ni se desbloquea la subida con una solicitud que el
    // usuario nunca llegó a ver, ni se pierde una factura que todavía puede
    // recuperar volviendo a Facturado.
    m_cfdiRequestGenerated = true;

    // Generada la solicitud, la factura apartada se descarta para siempre: en
    // autofactura la factura se sube DESPUÉS de la solicitud, así que una
    // elegida antes ya no cuenta, y la etiqueta vuelve a "Sin archivo", que
    // ahora es exacto. Solo si había una apartada: generar la solicitud otra
    // vez, con una factura subida después de la primera, no debe borrar de la
    // etiqueta el nombre de esa factura, que sí vale.
    if (!m_suspendedInvoiceFilePath.isEmpty()) {
        m_suspendedInvoiceFilePath.clear();
        m_invoiceFileLabel->setText(kNoInvoiceFileText);
        m_invoiceFileLabel->setToolTip(QString());
    }
    refreshInvoiceFileControls();
}

void VehicleDetailsView::reloadSubtypes()
{
    m_subtypeCombo->clear();

    const QVariant parentId = m_vehicleTypeCombo->currentData();
    if (!parentId.isValid())
        return;

    QList<application::CatalogOptionDto> subtypes;
    for (const application::CatalogOptionDto &option : std::as_const(m_subtypes)) {
        if (option.parentId == parentId.toInt())
            subtypes << option;
    }
    formsupport::fillCombo(m_subtypeCombo, subtypes);
}

void VehicleDetailsView::setLookups(const application::RegistrationLookupsDto &lookups)
{
    m_subtypes = lookups.vehicleSubtypes;
    formsupport::fillCombo(m_vehicleTypeCombo, lookups.vehicleTypes);
    formsupport::fillCombo(m_brandCombo, lookups.brands);

    m_brandCombo->setEditable(true);

    QCompleter* completer = m_brandCombo->completer();
    completer->setCompletionMode(QCompleter::PopupCompletion); // Muestra la lista desplegable al escribir
    completer->setFilterMode(Qt::MatchContains);
    m_brandCombo->setInsertPolicy(QComboBox::NoInsert);

    connect(m_brandCombo->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        QString wroteText = m_brandCombo->currentText();

        int indexValido = m_brandCombo->findText(wroteText, Qt::MatchExactly);

        // Una marca que no está en el catálogo deja el combo sin elegir (-1),
        // no en la primera marca: así el dominio la reporta como faltante y el
        // asistente marca el campo, en vez de guardar una marca que nadie
        // eligió.
        if (indexValido == -1) {
            m_brandCombo->setCurrentIndex(-1);
        }
    });

}

application::VehicleDetailsDto VehicleDetailsView::details() const
{
    application::VehicleDetailsDto dto;
    dto.acquisitionType = selectedAcquisitionType();
    dto.dealDate = m_dateEdit->date();
    dto.vehicleType = catalogRefFrom(m_vehicleTypeCombo);
    dto.subtype = catalogRefFrom(m_subtypeCombo);
    dto.brand = catalogRefFrom(m_brandCombo);
    dto.model = m_modelEdit->text();
    dto.yearModel = m_yearModelSpin->value();
    dto.color = m_colorEdit->text();
    dto.mileage = m_mileageSpin->value();
    dto.description = m_descriptionEdit->toPlainText();
    dto.motorNumber = m_motorNumberEdit->text();
    dto.serialNumber = m_serialNumberEdit->text();
    dto.repuve = m_repuveEdit->text();
    dto.plates = m_platesEdit->text();
    dto.platesHolder = m_platesHolderEdit->text();

    dto.counterparty.fullName = m_ownerNameEdit->text();
    dto.counterparty.nationalId = m_ownerIdEdit->text();
    dto.counterparty.streetAddress = m_ownerAddressEdit->text();
    dto.counterparty.suburb = m_ownerSuburbEdit->text();
    dto.counterparty.locality = m_ownerLocalityEdit->text();
    dto.counterparty.state = m_ownerStateEdit->text();
    dto.counterparty.postalCode = m_ownerPostalCodeEdit->text();

    // El userData del combo guarda el enum: no hay que traducir el texto de
    // vuelta ni depender de cómo esté escrita la etiqueta. Sin elegir queda
    // vacío, y el dominio lo reporta.
    if (m_invoiceTypeCombo->currentIndex() >= 0)
        dto.invoiceType = static_cast<domain::InvoiceType>(m_invoiceTypeCombo->currentData().toInt());
    dto.invoiceNumber = m_invoiceNumberEdit->text();
    dto.invoiceIssuer = m_invoiceIssuerEdit->text();
    dto.invoiceFilePath = m_invoiceFilePath;
    dto.maintenanceCost = m_maintenanceCostSpin->value();
    dto.observations = m_observationsEdit->toPlainText();

    // Los datos de las dos ramas viajan siempre: el servicio usa los de la
    // rama elegida.
    dto.purchasePrice = m_purchasePriceSpin->value();
    dto.salePrice = m_salePriceSpin->value();
    dto.paymentType = static_cast<domain::PaymentType>(m_paymentTypeCombo->currentData().toInt());
    dto.paymentMethod =
        static_cast<domain::PaymentMethod>(m_paymentMethodCombo->currentData().toInt());
    dto.basePrice = m_basePriceSpin->value();
    dto.commissionRate = m_commissionRateSpin->value();
    return dto;
}
