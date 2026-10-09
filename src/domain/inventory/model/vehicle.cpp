#include "domain/inventory/model/vehicle.h"

#include <QStringList>

namespace domain {
namespace {

// Topes de los VARCHAR de la tabla vehicles.
constexpr int kModelMaxLength = 50;
constexpr int kColorMaxLength = 30;
constexpr int kSerialNumberMaxLength = 50;
constexpr int kMotorNumberMaxLength = 50;
constexpr int kPlatesMaxLength = 20;
constexpr int kPlatesHolderMaxLength = 100;
constexpr int kRepuveMaxLength = 50;
constexpr int kInvoiceNumberMaxLength = 50;
constexpr int kInvoiceIssuerMaxLength = 150;

// Rango de año-modelo. Empieza antes de lo que ofrece el wizard a propósito:
// el dominio no debe ser más estrecho que la realidad, porque también lo usan
// las lecturas de datos viejos y cualquier importación futura. El margen
// hacia adelante existe porque los modelos se venden con el año siguiente.
constexpr int kMinYearModel = 1950;
constexpr int kYearModelFutureMargin = 2;

// Un kilometraje por encima de esto es un error de captura, no una unidad muy
// rodada.
constexpr int kMaxMileage = 2000000;

bool assignBounded(QString &target, const QString &value, int maxLength)
{
    const QString trimmed = value.trimmed();
    if (trimmed.length() > maxLength)
        return false;
    target = trimmed;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Template Method
// ---------------------------------------------------------------------------

ValidationResult Vehicle::validate() const
{
    ValidationResult result;

    if (m_serialNumber.isEmpty()) {
        result.addError(QStringLiteral("serialNumber"),
                        QStringLiteral("Captura el número de serie (VIN)."));
    }
    if (m_model.isEmpty()) {
        result.addError(QStringLiteral("model"), QStringLiteral("Captura el modelo."));
    }
    if (m_yearModel < kMinYearModel) {
        result.addError(QStringLiteral("yearModel"),
                        QStringLiteral("Indica el año del modelo."));
    }
    if (!m_brand.isValid()) {
        result.addError(QStringLiteral("brand"), QStringLiteral("Selecciona la marca."));
    }
    if (!m_vehicleType.isValid()) {
        result.addError(QStringLiteral("vehicleType"),
                        QStringLiteral("Selecciona el tipo de vehículo."));
    }

    // Obligatorios por decisión del negocio: son los campos que la pantalla
    // marca con asterisco. La regla vive aquí y no en la vista para que la
    // marca y la validación no vuelvan a contradecirse. El subtipo se puede
    // exigir siempre porque todos los tipos del catálogo tienen subtipos.
    if (!m_subtype.isValid()) {
        result.addError(QStringLiteral("subtype"), QStringLiteral("Selecciona el subtipo."));
    }
    if (m_motorNumber.isEmpty()) {
        result.addError(QStringLiteral("motorNumber"),
                        QStringLiteral("Captura el número de motor."));
    }
    if (m_repuve.isEmpty()) {
        result.addError(QStringLiteral("repuve"), QStringLiteral("Captura el número de REPUVE."));
    }
    if (m_plates.isEmpty()) {
        result.addError(QStringLiteral("plates"), QStringLiteral("Captura las placas."));
    }
    if (m_platesHolder.isEmpty()) {
        result.addError(QStringLiteral("platesHolder"),
                        QStringLiteral("Captura el titular de las placas."));
    }
    // Una unidad "No Facturada" no tiene factura, así que no puede tener
    // número: exigirlo obligaría a inventar uno.
    if (m_invoiceType != InvoiceType::NoFacturado && m_invoiceNumber.isEmpty()) {
        result.addError(QStringLiteral("invoiceNumber"),
                        QStringLiteral("Captura el número de factura."));
    }

    // El valor por omisión de m_invoiceType no puede ser válido para las dos
    // ramas a la vez, porque sus CHECK son conjuntos disjuntos: una
    // consignación recién construida arranca con un tipo que le corresponde a
    // una adquisición. Comprobarlo aquí evita que ese valor llegue a la base
    // y reviente la restricción desde el hilo de guardado, ya con los
    // archivos copiados a disco.
    if (!acceptsInvoiceType(m_invoiceType)) {
        result.addError(QStringLiteral("invoiceType"),
                        QStringLiteral("Selecciona el tipo de factura."));
    }

    result.merge(m_counterparty.validate(), QStringLiteral("counterparty"));

    // Aquí entra lo propio de cada rama: precios, forma de pago, comisión.
    collectSpecificErrors(result);

    return result;
}

// ---------------------------------------------------------------------------
// Identidad
// ---------------------------------------------------------------------------

int Vehicle::folio() const
{
    return m_folio;
}

bool Vehicle::isPersisted() const
{
    return m_folio > 0;
}

void Vehicle::assignFolio(int folio)
{
    m_folio = folio;
}

// ---------------------------------------------------------------------------
// Datos de la unidad
// ---------------------------------------------------------------------------

VehicleStatus Vehicle::status() const
{
    return m_status;
}

void Vehicle::setStatus(VehicleStatus value)
{
    m_status = value;
}

const QString &Vehicle::serialNumber() const
{
    return m_serialNumber;
}

bool Vehicle::setSerialNumber(const QString &value)
{
    // Se normaliza a mayúsculas pero NO se exige el formato de 17 caracteres
    // del estándar internacional: los seminuevos mexicanos viejos traen VIN
    // más cortos, y rechazarlos bloquearía una captura legítima.
    const QString normalized = value.trimmed().toUpper();
    if (normalized.length() > kSerialNumberMaxLength)
        return false;
    m_serialNumber = normalized;
    return true;
}

const QString &Vehicle::model() const
{
    return m_model;
}

bool Vehicle::setModel(const QString &value)
{
    return assignBounded(m_model, value, kModelMaxLength);
}

int Vehicle::yearModel() const
{
    return m_yearModel;
}

bool Vehicle::setYearModel(int value)
{
    const int maxYear = QDate::currentDate().year() + kYearModelFutureMargin;
    if (value < kMinYearModel || value > maxYear)
        return false;
    m_yearModel = value;
    return true;
}

int Vehicle::mileage() const
{
    return m_mileage;
}

bool Vehicle::setMileage(int value)
{
    if (value < 0 || value > kMaxMileage)
        return false;
    m_mileage = value;
    return true;
}

const CatalogRef &Vehicle::vehicleType() const
{
    return m_vehicleType;
}

void Vehicle::setVehicleType(const CatalogRef &value)
{
    // Se aceptan referencias inválidas: un combo puede estar vacío mientras
    // el usuario captura, y ese estado tiene que poder representarse. Que
    // sean obligatorias al guardar lo decide validate().
    m_vehicleType = value;
}

const CatalogRef &Vehicle::subtype() const
{
    return m_subtype;
}

void Vehicle::setSubtype(const CatalogRef &value)
{
    m_subtype = value;
}

const CatalogRef &Vehicle::brand() const
{
    return m_brand;
}

void Vehicle::setBrand(const CatalogRef &value)
{
    m_brand = value;
}

const QString &Vehicle::color() const
{
    return m_color;
}

bool Vehicle::setColor(const QString &value)
{
    return assignBounded(m_color, value, kColorMaxLength);
}

const QString &Vehicle::motorNumber() const
{
    return m_motorNumber;
}

bool Vehicle::setMotorNumber(const QString &value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized.length() > kMotorNumberMaxLength)
        return false;
    m_motorNumber = normalized;
    return true;
}

const QString &Vehicle::plates() const
{
    return m_plates;
}

bool Vehicle::setPlates(const QString &value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized.length() > kPlatesMaxLength)
        return false;
    m_plates = normalized;
    return true;
}

const QString &Vehicle::platesHolder() const
{
    return m_platesHolder;
}

bool Vehicle::setPlatesHolder(const QString &value)
{
    return assignBounded(m_platesHolder, value, kPlatesHolderMaxLength);
}

const QString &Vehicle::repuve() const
{
    return m_repuve;
}

bool Vehicle::setRepuve(const QString &value)
{
    return assignBounded(m_repuve, value, kRepuveMaxLength);
}

const QString &Vehicle::description() const
{
    return m_description;
}

void Vehicle::setDescription(const QString &value)
{
    // Columna TEXT: no hay tope que respetar.
    m_description = value.trimmed();
}

QDate Vehicle::dealDate() const
{
    return m_dealDate;
}

bool Vehicle::setDealDate(QDate value)
{
    if (!value.isValid() || value > QDate::currentDate())
        return false;
    m_dealDate = value;
    return true;
}

QDate Vehicle::addedDate() const
{
    return m_addedDate;
}

void Vehicle::setAddedDate(QDate value)
{
    if (value.isValid())
        m_addedDate = value;
}

// ---------------------------------------------------------------------------
// Contraparte
// ---------------------------------------------------------------------------

const Counterparty &Vehicle::counterparty() const
{
    return m_counterparty;
}

void Vehicle::setCounterparty(const Counterparty &value)
{
    m_counterparty = value;
}

void Vehicle::assignCounterpartyId(int id)
{
    m_counterparty.assignId(id);
}

// ---------------------------------------------------------------------------
// Factura y costos
// ---------------------------------------------------------------------------

InvoiceType Vehicle::invoiceType() const
{
    return m_invoiceType;
}

bool Vehicle::setInvoiceType(InvoiceType value)
{
    // Validación polimórfica dentro de un setter de la base: cada rama tiene
    // su propio conjunto de valores permitidos, y son disjuntos.
    if (!acceptsInvoiceType(value))
        return false;
    m_invoiceType = value;
    return true;
}

const QString &Vehicle::invoiceNumber() const
{
    return m_invoiceNumber;
}

bool Vehicle::setInvoiceNumber(const QString &value)
{
    return assignBounded(m_invoiceNumber, value, kInvoiceNumberMaxLength);
}

const QString &Vehicle::invoiceIssuer() const
{
    return m_invoiceIssuer;
}

bool Vehicle::setInvoiceIssuer(const QString &value)
{
    return assignBounded(m_invoiceIssuer, value, kInvoiceIssuerMaxLength);
}

double Vehicle::maintenanceCost() const
{
    return m_maintenanceCost;
}

bool Vehicle::setMaintenanceCost(double value)
{
    if (value < 0.0)
        return false;
    m_maintenanceCost = value;
    return true;
}

const QString &Vehicle::observations() const
{
    return m_observations;
}

void Vehicle::setObservations(const QString &value)
{
    m_observations = value.trimmed();
}

const QString &Vehicle::invoiceFilePath() const
{
    static const QString empty;
    return empty;
}

void Vehicle::setInvoiceFilePath(const QString &)
{
    // Sin efecto: la rama que no guarda factura simplemente lo ignora.
}

// ---------------------------------------------------------------------------
// Composición
// ---------------------------------------------------------------------------

const VehicleConditions &Vehicle::conditions() const
{
    return m_conditions;
}

void Vehicle::setConditions(const VehicleConditions &value)
{
    m_conditions = value;
}

const Inspection &Vehicle::inspection() const
{
    return m_inspection;
}

void Vehicle::setInspection(const Inspection &value)
{
    m_inspection = value;
}

const QList<VehicleImage> &Vehicle::images() const
{
    return m_images;
}

bool Vehicle::addImage(const VehicleImage &image)
{
    if (image.path.trimmed().isEmpty())
        return false;

    if (image.isPrimary) {
        for (const VehicleImage &existing : m_images) {
            if (existing.isPrimary)
                return false;
        }
    }

    m_images.append(image);
    return true;
}

void Vehicle::clearImages()
{
    m_images.clear();
}

const QList<VehicleDocument> &Vehicle::documents() const
{
    return m_documents;
}

bool Vehicle::addDocument(const VehicleDocument &document)
{
    if (document.documentType.trimmed().isEmpty())
        return false;

    for (const VehicleDocument &existing : m_documents) {
        if (existing.documentType == document.documentType)
            return false;
    }

    m_documents.append(document);
    return true;
}

void Vehicle::clearDocuments()
{
    m_documents.clear();
}

bool Vehicle::setImageStoredPath(int index, const QString &storedPath)
{
    if (index < 0 || index >= m_images.size())
        return false;
    m_images[index].path = storedPath;
    return true;
}

bool Vehicle::setDocumentStoredPath(int index, const QString &storedPath)
{
    if (index < 0 || index >= m_documents.size())
        return false;
    m_documents[index].path = storedPath;
    return true;
}

// ---------------------------------------------------------------------------
// Consultas de negocio
// ---------------------------------------------------------------------------

QString Vehicle::displayTitle() const
{
    QStringList parts;
    if (!m_brand.name.isEmpty())
        parts << m_brand.name;
    if (!m_model.isEmpty())
        parts << m_model;
    if (m_yearModel > 0)
        parts << QString::number(m_yearModel);
    return parts.join(QLatin1Char(' '));
}

bool Vehicle::isAvailable() const
{
    return m_status == VehicleStatus::Disponible;
}

bool Vehicle::hasFaults() const
{
    return m_inspection.faultCount() > 0;
}

// ---------------------------------------------------------------------------
// Contrato
// ---------------------------------------------------------------------------

bool Vehicle::canGenerateContract() const
{
    // Por omisión, no. Así una rama nueva no empieza a emitir documentos
    // legales por el simple hecho de heredar: quien la agregue tiene que
    // decidir a conciencia si le corresponde contrato y con qué título.
    return false;
}

QString Vehicle::contractTitle() const
{
    return QString();
}

double Vehicle::contractAmount() const
{
    return 0.0;
}

QMap<QString, QString> Vehicle::contractPlaceholders() const
{
    QMap<QString, QString> values;

    values.insert(QStringLiteral("titulo_contrato"), contractTitle());

    values.insert(QStringLiteral("vendedor_nombre"), m_counterparty.fullName());
    values.insert(QStringLiteral("vendedor_domicilio"), m_counterparty.formattedAddress());
    values.insert(QStringLiteral("vendedor_identificacion"), m_counterparty.nationalId());

    values.insert(QStringLiteral("marca"), m_brand.name);
    values.insert(QStringLiteral("modelo_anio"), QString::number(m_yearModel));
    values.insert(QStringLiteral("tipo"), m_model);
    values.insert(QStringLiteral("no_serie"), m_serialNumber);
    values.insert(QStringLiteral("no_motor"), m_motorNumber);
    values.insert(QStringLiteral("no_placas"),
                  m_plates.isEmpty() ? QStringLiteral("SIN PLACA") : m_plates);
    values.insert(QStringLiteral("color"), m_color);
    values.insert(QStringLiteral("fecha"), m_dealDate.toString(QStringLiteral("dd/MM/yyyy")));

    return values;
}

} // namespace domain

namespace domain {

std::optional<ContractData> Vehicle::contractData() const
{
    if (!canGenerateContract())
        return std::nullopt;

    ContractData data;
    data.title = contractTitle();
    data.amount = contractAmount();
    data.placeholders = contractPlaceholders();

    // Los documentos que acompañan la operación, en texto plano: el adaptador
    // del PDF los escapa y los convierte en lista.
    if (!invoiceNumber().trimmed().isEmpty()) {
        data.documentLines << QStringLiteral("FACTURA %1 No. %2 EXPEDIDA POR %3")
                                  .arg(toDbString(invoiceType()), invoiceNumber(), invoiceIssuer());
    }
    for (const VehicleDocument &document : documents())
        data.documentLines << document.documentType;

    data.fileNameHint = serialNumber();
    return data;
}

} // namespace domain
