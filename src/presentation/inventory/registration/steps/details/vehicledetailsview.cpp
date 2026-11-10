#include "presentation/inventory/registration/steps/details/vehicledetailsview.h"
#include "ui_vehicledetailsview.h"

#include "presentation/common/forms/formsupport.h"
#include "presentation/inventory/acquisition/acquisitiontermssection.h"
#include "presentation/inventory/consignment/consignmenttermssection.h"
#include "presentation/inventory/maintenance/maintenancehistorytablewidget.h"

#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>

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
    // todo lo demás: qué sección de la rama se ve y qué valores admite el
    // combo de factura. Cada rama es una sección promovida que ocupa una fila
    // completa de la rejilla (acquisitionSection y consignmentSection): al
    // ocultar una, su fila colapsa entera en vez de dejar huecos a un lado.
    // Las tres rejillas (la de la tarjeta y las de las dos secciones) estiran
    // igual sus columnas (0,1,0,1) para poder alinearse; ver showEvent().
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

    setUpBrandSearch();

    // Los botones de la factura viven en la sección de Adquisición.
    connect(ui->acquisitionSection, &AcquisitionTermsSection::browseInvoiceRequested, this,
            &VehicleDetailsView::browseInvoiceRequested);
    connect(ui->acquisitionSection, &AcquisitionTermsSection::cfdiRequestRequested, this,
            &VehicleDetailsView::cfdiRequestRequested);
    connect(ui->acquisitionSection, &AcquisitionTermsSection::viewInvoiceRequested, this,
            &VehicleDetailsView::viewInvoiceRequested);

    // El orden de tabulación cruza tres formularios, así que el .ui de este
    // paso solo lo declara hasta "Expidió Factura" y el resto se encadena
    // aquí: los campos de las dos secciones y después los comunes del final.
    QList<QWidget *> focusChain{ui->invoiceIssuerEdit};
    focusChain << ui->acquisitionSection->focusOrder() << ui->consignmentSection->focusOrder();
    focusChain << ui->maintenanceCostSpin << ui->observationsEdit;
    for (qsizetype i = 0; i + 1 < focusChain.size(); ++i)
        QWidget::setTabOrder(focusChain.at(i), focusChain.at(i + 1));

    // El historial de mantenimientos va entre las dos tarjetas, y el Tab
    // también: de la descripción a sus botones y de ahí a Tipo Operación.
    // Mientras está oculto (en el alta, siempre), el Tab se lo salta. Va oculto
    // desde aquí porque solo el expediente lo muestra, y solo a quien su rol
    // se lo permite.
    QList<QWidget *> historyChain{ui->descriptionEdit};
    historyChain << ui->maintenanceHistorySection->focusOrder() << ui->acquisitionTypeCombo;
    for (qsizetype i = 0; i + 1 < historyChain.size(); ++i)
        QWidget::setTabOrder(historyChain.at(i), historyChain.at(i + 1));
    ui->maintenanceHistorySection->setVisible(false);

    // Bajo cada campo, el lugar de sus mensajes de error (también los de las
    // secciones). Sin errores no ocupa espacio.
    formsupport::addFieldErrorLabels(this);

    connect(ui->vehicleTypeCombo, &QComboBox::currentIndexChanged, this, &VehicleDetailsView::reloadSubtypes);
    connect(ui->acquisitionTypeCombo, &QComboBox::currentIndexChanged, this,
            &VehicleDetailsView::onAcquisitionTypeChanged);
    connect(ui->invoiceTypeCombo, &QComboBox::currentIndexChanged, this,
            &VehicleDetailsView::invoiceTypeChanged);
    // Deja la vista coherente con la rama seleccionada por omisión.
    onAcquisitionTypeChanged();

    // Cualquier cambio se avisa; el presenter decide cuándo revalidar. Solo
    // dentro de las dos tarjetas (las secciones de cada rama van en la
    // segunda): lo que se capture en el historial de mantenimientos no es una
    // edición de la unidad.
    for (QFrame *card : {ui->generalInfoCard, ui->ownerAndAcquisitionCard})
        formsupport::watchEdits(card, this, [this] { emit edited(); });
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

    ui->acquisitionSection->setVisible(isAcquisition);
    ui->consignmentSection->setVisible(!isAcquisition);

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

presentation::IAcquisitionTermsView &VehicleDetailsView::acquisitionTerms()
{
    return *ui->acquisitionSection;
}

MaintenanceHistoryTableWidget &VehicleDetailsView::maintenanceHistory()
{
    return *ui->maintenanceHistorySection;
}

void VehicleDetailsView::setMaintenanceHistoryVisible(bool visible)
{
    ui->maintenanceHistorySection->setVisible(visible);
}

void VehicleDetailsView::setDealTermsVisible(bool visible)
{
    ui->ownerAndAcquisitionCard->setVisible(visible);
}

void VehicleDetailsView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_columnsAligned)
        return;
    m_columnsAligned = true;

    // Las secciones de cada rama tienen su propia rejilla, y sin esto sus
    // columnas tomarían el ancho de sus etiquetas, no el de las de la tarjeta.
    // Va aquí y no en el constructor porque el ancho de una etiqueta depende
    // de la hoja de estilos, y al mostrarse por primera vez el paso ya se
    // pulió.
    formsupport::alignGridColumns({ui->ownerAndAcquisitionGrid,
                                   ui->acquisitionSection->grid(),
                                   ui->consignmentSection->grid()});
}

QSize VehicleDetailsView::minimumSizeHint() const
{
    QSize size = QWidget::minimumSizeHint();
    // Con mensajes de error que se parten en renglones, el layout calcula su
    // alto según el ancho, y el área con scroll ya toma el alto al ancho
    // real. El natural se calcula a otro ancho y dejaría un hueco abajo.
    if (layout() && !layout()->hasHeightForWidth())
        size.setHeight(qMax(size.height(), sizeHint().height()));
    return size;
}

QList<domain::ValidationError>
VehicleDetailsView::showFieldErrors(const QList<domain::ValidationError> &errors)
{
    return formsupport::showFieldErrors(this, errors);
}

bool VehicleDetailsView::focusField(const QString &field)
{
    return formsupport::focusField(this, field);
}

void VehicleDetailsView::setReadOnly(bool readOnly, const QStringList &lockedFields)
{
    // Solo las dos tarjetas (las secciones de cada rama van en la segunda): el
    // historial de mantenimientos, que va entre ellas, no es parte de la
    // captura de la unidad.
    for (QFrame *card : {ui->generalInfoCard, ui->ownerAndAcquisitionCard})
        formsupport::setReadOnly(card, readOnly, lockedFields);
    // Los botones de la factura (CFDI y subir) se ocultan en consulta.
    ui->acquisitionSection->setReadOnly(readOnly);
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
}

void VehicleDetailsView::showDetails(int folio, const application::VehicleDetailsDto &details)
{
    formsupport::showText(ui->folioEdit, QString::number(folio));

    // El orden importa: la rama decide qué tipos de factura ofrece su combo,
    // y el tipo de vehículo, qué subtipos. Cada uno repuebla al siguiente, así
    // que se eligen de arriba abajo y lo demás va al final.
    formsupport::selectEnum(ui->acquisitionTypeCombo, details.acquisitionType);
    formsupport::selectOption(ui->vehicleTypeCombo, details.vehicleType);
    formsupport::selectOption(ui->subtypeCombo, details.subtype);
    formsupport::selectOption(ui->brandCombo, details.brand);
    formsupport::selectEnum(ui->invoiceTypeCombo, details.invoiceType);

    formsupport::showDate(ui->dateEdit, details.dealDate);
    formsupport::showText(ui->modelEdit, details.model);
    formsupport::showValue(ui->yearModelSpin, details.yearModel);
    formsupport::showText(ui->colorEdit, details.color);
    formsupport::showValue(ui->mileageSpin, details.mileage);
    ui->descriptionEdit->setPlainText(details.description);
    formsupport::showText(ui->motorNumberEdit, details.motorNumber);
    formsupport::showText(ui->serialNumberEdit, details.serialNumber);
    formsupport::showText(ui->repuveEdit, details.repuve);
    formsupport::showText(ui->platesEdit, details.plates);
    formsupport::showText(ui->platesHolderEdit, details.platesHolder);

    formsupport::showText(ui->ownerNameEdit, details.counterparty.fullName);
    formsupport::showText(ui->ownerIdEdit, details.counterparty.nationalId);
    formsupport::showText(ui->ownerAddressEdit, details.counterparty.streetAddress);
    formsupport::showText(ui->ownerSuburbEdit, details.counterparty.suburb);
    formsupport::showText(ui->ownerLocalityEdit, details.counterparty.locality);
    formsupport::showText(ui->ownerStateEdit, details.counterparty.state);
    formsupport::showText(ui->ownerPostalCodeEdit, details.counterparty.postalCode);

    formsupport::showText(ui->invoiceNumberEdit, details.invoiceNumber);
    formsupport::showText(ui->invoiceIssuerEdit, details.invoiceIssuer);
    showMaintenanceCost(details.maintenanceCost);
    ui->observationsEdit->setPlainText(details.observations);

    ui->acquisitionSection->showTerms(details);
    ui->consignmentSection->showTerms(details);
}

void VehicleDetailsView::showMaintenanceCost(double cost)
{
    formsupport::showValue(ui->maintenanceCostSpin, cost);
}

void VehicleDetailsView::setUpBrandSearch()
{
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
    ui->acquisitionSection->fill(dto);
    ui->consignmentSection->fill(dto);
    return dto;
}
