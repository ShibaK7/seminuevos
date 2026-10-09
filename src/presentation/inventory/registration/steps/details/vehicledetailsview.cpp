#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "ui_vehicledetailsview.h"

#include "presentation/common/forms/formsupport.h"

#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QUrl>

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

} // namespace

VehicleDetailsView::VehicleDetailsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VehicleDetailsView)
{
    // Sin aviso propio: los errores de este paso los explica el aviso del
    // asistente, y los campos que fallan se marcan por su propiedad "field".
    //
    // El .ui arma las dos tarjetas, cada una con su QGridLayout de cuatro
    // columnas (etiqueta, campo, etiqueta, campo). Margen de 16 px: Folio es
    // la primera fila del formulario -- que quede pegada arriba del
    // contenedor, con solo unos pocos px de aire, no el margen grande por
    // defecto del layout.
    //
    // Cada widget de captura lleva en "field" la clave con la que el dominio
    // reporta sus errores (la misma que usan Vehicle::validate() y los
    // rechazos de VehicleBuilder). Con ella el asistente marca en rojo los que
    // fallan y le da el foco al primero, sin que esta vista traduzca errores.
    // También la llevan los campos que hoy no tienen regla: si el dominio
    // agrega una, la marca funciona sin tocar esta vista. Los datos de la
    // contraparte llevan el prefijo "counterparty.": así los reporta
    // Vehicle::validate() al juntar la validación de Counterparty.
    //
    // Las etiquetas con asterisco llevan el mismo texto que arma
    // formsupport::requiredLabel(). Folio va sin asterisco: lo asigna la base
    // al guardar, el usuario no lo captura.
    //
    // En la segunda tarjeta, el tipo de operación va primero porque condiciona
    // todo lo demás: qué campos de precio se piden y qué valores admite el
    // combo de factura. Los campos propios de cada rama ocupan FILAS
    // COMPLETAS, no medias filas compartidas con campos comunes: así, al
    // ocultar una rama, sus filas colapsan enteras en vez de dejar huecos a un
    // lado. Si se mueven en Designer, hay que conservar eso.
    //
    // El archivo de factura solo existe en la compra: el esquema pone
    // invoice_file_path únicamente en vehicle_acquisitions. Va envuelto en un
    // widget (invoiceFileWidget) porque un QHBoxLayout suelto no se puede
    // ocultar de una pieza. Sus botones no llevan "field": abren un diálogo en
    // vez de capturar un dato, y el tooltip del de subir explica cuándo está
    // bloqueado, algo que el tooltip de un error taparía. El de CFDI va antes
    // que el de subir porque en autofactura hay que usarlo primero: leída de
    // izquierda a derecha, la fila repite la secuencia que se exige.
    ui->setupUi(this);

    // Las tarjetas se llaman cardPanel en tiempo de ejecución porque así las
    // encuentra el QSS global (QFrame#cardPanel). En el .ui no pueden llevar
    // ese nombre las dos: un formulario no admite nombres repetidos (uic deja
    // la segunda como "cardPanel1" y Designer la renombra al abrirla), así
    // que ahí tienen nombre propio y se renombran aquí, antes de que la hoja
    // de estilos las pula. La sombra tampoco cabe en el .ui: Designer no la
    // puede declarar.
    for (QFrame *card : {ui->generalInfoCard, ui->ownerAndAcquisitionCard}) {
        card->setObjectName(QStringLiteral("cardPanel"));
        formsupport::applyFloatingShadow(card);
    }

    // La fecha de hoy y los topes que dependen de ella no caben en el .ui,
    // que solo guarda valores fijos.
    ui->dateEdit->setMaximumDate(QDate::currentDate());
    ui->dateEdit->setDate(QDate::currentDate());
    ui->yearModelSpin->setRange(1980, QDate::currentDate().year() + 1);
    ui->yearModelSpin->setValue(QDate::currentDate().year());

    // Los combos de valores cerrados se llenan desde el dominio. El de
    // factura se llena en onAcquisitionTypeChanged(): los valores válidos
    // dependen de la rama y los conjuntos son disjuntos.
    for (domain::AcquisitionType value : domain::allAcquisitionTypes())
        ui->acquisitionTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    for (domain::PaymentType value : domain::allPaymentTypes())
        ui->paymentTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    for (domain::PaymentMethod value : domain::allPaymentMethods())
        ui->paymentMethodCombo->addItem(domain::displayLabel(value), static_cast<int>(value));

    // Solo aplica a la autofactura, y en ella hay que usarlo antes de poder
    // subir la factura. Arranca oculto; ni la visibilidad ni el bloqueo se
    // deciden aquí: los decide el presenter (InvoiceAttachment) y llegan por
    // showInvoiceAttachment().
    ui->cfdiRequestButton->setVisible(false);
    connect(ui->cfdiRequestButton, &QPushButton::clicked, this, &VehicleDetailsView::cfdiRequestRequested);
    connect(ui->invoiceUploadButton, &QPushButton::clicked, this, &VehicleDetailsView::browseInvoiceRequested);

    // --- Solo Adquisición ---
    // Las reglas de los precios y del tope de pago en efectivo no van aquí:
    // son del dominio (AcquiredVehicle), que conoce la UMA vigente y reporta
    // cada error con la clave del campo que lo provoca, y el asistente marca
    // ese campo. Una revisión propia en esta vista repetía el tope con otro
    // criterio, se pintaba en rojo desde que se abría el paso y movía el foco
    // por su cuenta.
    m_acquisitionOnlyWidgets << ui->invoiceFileRowLabel << ui->invoiceFileWidget
                             << ui->purchasePriceLabel << ui->purchasePriceSpin
                             << ui->paymentTypeLabel << ui->paymentTypeCombo
                             << ui->paymentMethodLabel << ui->paymentMethodCombo
                             << ui->salePriceLabel << ui->salePriceSpin;

    // --- Solo Consignación ---
    // No hay precio de venta que capturar: sale de base + comisión, igual que
    // la columna generada de vehicle_consignments. El precio base es
    // obligatorio en consignación: es lo que se le entrega al propietario y lo
    // que imprime el contrato.
    m_consignmentOnlyWidgets << ui->basePriceLabel << ui->basePriceSpin
                             << ui->commissionRateLabel << ui->commissionRateSpin;

    connect(ui->vehicleTypeCombo, &QComboBox::currentIndexChanged, this, &VehicleDetailsView::reloadSubtypes);
    connect(ui->acquisitionTypeCombo, &QComboBox::currentIndexChanged, this,
            &VehicleDetailsView::onAcquisitionTypeChanged);
    connect(ui->invoiceTypeCombo, &QComboBox::currentIndexChanged, this,
            &VehicleDetailsView::invoiceTypeChanged);
    // Deja la vista coherente con la rama seleccionada por omisión.
    onAcquisitionTypeChanged();

    // Cualquier cambio se avisa; el presenter decide cuándo revalidar.
    formsupport::watchEdits(this, this, [this] { emit edited(); });
}

VehicleDetailsView::~VehicleDetailsView()
{
    delete ui;
}

domain::AcquisitionType VehicleDetailsView::selectedAcquisitionType() const
{
    return static_cast<domain::AcquisitionType>(ui->acquisitionTypeCombo->currentData().toInt());
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
    // de la otra rama haría que el INSERT violara la restricción.
    const QString previous = ui->invoiceTypeCombo->currentText();
    ui->invoiceTypeCombo->clear();
    for (domain::InvoiceType value : domain::invoiceTypesFor(type))
        ui->invoiceTypeCombo->addItem(domain::displayLabel(value), static_cast<int>(value));
    // Si el valor anterior sigue siendo válido en la rama nueva, se conserva.
    const int restored = ui->invoiceTypeCombo->findText(previous);
    if (restored >= 0)
        ui->invoiceTypeCombo->setCurrentIndex(restored);

    // La contraparte se llama distinto en cada rama, así que su etiqueta se
    // actualiza junto con el resto. En una compra la contraparte vende la
    // unidad; en una consignación sigue siendo su dueña.
    ui->counterpartyLabel->setText(isAcquisition ? QStringLiteral("Vendedor: <span style='color: #D90429; font-weight: bold;'>*</span>")
                                                 : QStringLiteral("Propietario: <span style='color: #D90429; font-weight: bold;'>*</span>"));
}

void VehicleDetailsView::showInvoiceAttachment(const presentation::InvoiceAttachmentState &state)
{
    // Ocultar el botón por su cuenta no choca con que la fila entera se oculte
    // en la consignación: un hijo ocultado explícitamente sigue oculto cuando
    // su padre se vuelve a mostrar, así que un mecanismo no deshace al otro.
    ui->cfdiRequestButton->setVisible(state.cfdiButtonVisible);
    ui->invoiceUploadButton->setEnabled(state.uploadEnabled);
    ui->invoiceUploadButton->setToolTip(state.uploadToolTip);
    ui->invoiceFileLabel->setText(state.fileLabel);
    ui->invoiceFileLabel->setToolTip(state.fileToolTip);
}

QString VehicleDetailsView::askInvoiceFile()
{
    return QFileDialog::getOpenFileName(this, QStringLiteral("Seleccionar factura"));
}

bool VehicleDetailsView::openDocument(const QString &path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void VehicleDetailsView::showWarning(const QString &title, const QString &message)
{
    QMessageBox::warning(this, title, message);
}

void VehicleDetailsView::showFieldErrors(const QList<domain::ValidationError> &errors)
{
    formsupport::showFieldErrors(this, errors);
}

bool VehicleDetailsView::focusField(const QString &field)
{
    return formsupport::focusField(this, field);
}

void VehicleDetailsView::reloadSubtypes()
{
    ui->subtypeCombo->clear();

    const QVariant parentId = ui->vehicleTypeCombo->currentData();
    if (!parentId.isValid())
        return;

    QList<application::CatalogOptionDto> subtypes;
    for (const application::CatalogOptionDto &option : std::as_const(m_subtypes)) {
        if (option.parentId == parentId.toInt())
            subtypes << option;
    }
    formsupport::fillCombo(ui->subtypeCombo, subtypes);
}

void VehicleDetailsView::setLookups(const application::RegistrationLookupsDto &lookups)
{
    m_subtypes = lookups.vehicleSubtypes;
    formsupport::fillCombo(ui->vehicleTypeCombo, lookups.vehicleTypes);
    formsupport::fillCombo(ui->brandCombo, lookups.brands);

    ui->brandCombo->setEditable(true);

    QCompleter* completer = ui->brandCombo->completer();
    completer->setCompletionMode(QCompleter::PopupCompletion); // Muestra la lista desplegable al escribir
    completer->setFilterMode(Qt::MatchContains);
    ui->brandCombo->setInsertPolicy(QComboBox::NoInsert);

    connect(ui->brandCombo->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        QString wroteText = ui->brandCombo->currentText();

        int indexValido = ui->brandCombo->findText(wroteText, Qt::MatchExactly);

        // Una marca que no está en el catálogo deja el combo sin elegir (-1),
        // no en la primera marca: así el dominio la reporta como faltante y el
        // asistente marca el campo, en vez de guardar una marca que nadie
        // eligió.
        if (indexValido == -1) {
            ui->brandCombo->setCurrentIndex(-1);
        }
    });

}

application::VehicleDetailsDto VehicleDetailsView::details() const
{
    application::VehicleDetailsDto dto;
    dto.acquisitionType = selectedAcquisitionType();
    dto.dealDate = ui->dateEdit->date();
    dto.vehicleType = catalogRefFrom(ui->vehicleTypeCombo);
    dto.subtype = catalogRefFrom(ui->subtypeCombo);
    dto.brand = catalogRefFrom(ui->brandCombo);
    dto.model = ui->modelEdit->text();
    dto.yearModel = ui->yearModelSpin->value();
    dto.color = ui->colorEdit->text();
    dto.mileage = ui->mileageSpin->value();
    dto.description = ui->descriptionEdit->toPlainText();
    dto.motorNumber = ui->motorNumberEdit->text();
    dto.serialNumber = ui->serialNumberEdit->text();
    dto.repuve = ui->repuveEdit->text();
    dto.plates = ui->platesEdit->text();
    dto.platesHolder = ui->platesHolderEdit->text();

    dto.counterparty.fullName = ui->ownerNameEdit->text();
    dto.counterparty.nationalId = ui->ownerIdEdit->text();
    dto.counterparty.streetAddress = ui->ownerAddressEdit->text();
    dto.counterparty.suburb = ui->ownerSuburbEdit->text();
    dto.counterparty.locality = ui->ownerLocalityEdit->text();
    dto.counterparty.state = ui->ownerStateEdit->text();
    dto.counterparty.postalCode = ui->ownerPostalCodeEdit->text();

    // El userData del combo guarda el enum: no hay que traducir el texto de
    // vuelta ni depender de cómo esté escrita la etiqueta. Sin elegir queda
    // vacío, y el dominio lo reporta.
    if (ui->invoiceTypeCombo->currentIndex() >= 0)
        dto.invoiceType = static_cast<domain::InvoiceType>(ui->invoiceTypeCombo->currentData().toInt());
    dto.invoiceNumber = ui->invoiceNumberEdit->text();
    dto.invoiceIssuer = ui->invoiceIssuerEdit->text();
    dto.maintenanceCost = ui->maintenanceCostSpin->value();
    dto.observations = ui->observationsEdit->toPlainText();

    // Los datos de las dos ramas viajan siempre: el servicio usa los de la
    // rama elegida.
    dto.purchasePrice = ui->purchasePriceSpin->value();
    dto.salePrice = ui->salePriceSpin->value();
    dto.paymentType = static_cast<domain::PaymentType>(ui->paymentTypeCombo->currentData().toInt());
    dto.paymentMethod =
        static_cast<domain::PaymentMethod>(ui->paymentMethodCombo->currentData().toInt());
    dto.basePrice = ui->basePriceSpin->value();
    dto.commissionRate = ui->commissionRateSpin->value();
    return dto;
}
