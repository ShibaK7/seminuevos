#ifndef DOMAIN_ENUMS_H
#define DOMAIN_ENUMS_H

#include <QList>
#include <QString>

#include <optional>

// Conjuntos cerrados de valores del dominio. Cada uno corresponde a un CHECK
// del esquema.
//
// Por qué enum y no QString validado: hoy los pasos del wizard copian el
// currentText() de un combo directo a una columna con CHECK. Eso funciona
// por coincidencia -- basta que alguien cambie una etiqueta de la UI, o que
// la envuelva en tr() (el proyecto tiene qt_create_translation activo), para
// que el INSERT reviente en tiempo de ejecución con un error de restricción.
// Con enum, ese error deja de compilar.
//
// De ahí que toDbString() y displayLabel() sean funciones SEPARADAS aunque
// hoy devuelvan el mismo texto: la primera es el contrato con PostgreSQL y
// nunca debe traducirse; la segunda es lo que lee el usuario y algún día
// podría hacerlo.
namespace domain {

// Discriminador del Class Table Inheritance: decide si el vehículo vive en
// vehicle_acquisitions o en vehicle_consignments.
enum class AcquisitionType {
    Adquisicion,
    Consignacion,
};

enum class VehicleStatus {
    Disponible,
    Apartado,
    Vendido,
};

// Los cuatro valores conviven en el enum, pero los CHECK de las dos
// subtablas son conjuntos DISJUNTOS: Facturado/Autofactura solo valen en
// adquisición, y FacturadoReal/NoFacturado solo en consignación. Quién
// acepta cuál lo decide Vehicle::acceptsInvoiceType(), no este archivo.
enum class InvoiceType {
    Facturado,
    Autofactura,
    FacturadoReal,
    NoFacturado,
};

enum class PaymentType {
    Contado,
    Credito,
};

enum class PaymentMethod {
    Efectivo,
    Transferencia,
};

enum class Transmission {
    Automatica,
    Manual,
};

enum class WindowRegulators {
    Manuales,
    ElectricosTradicionales,
    ElectricosInteligentes,
};

enum class AirConditioning {
    Automatico,
    Manual,
};

// --- Serialización hacia PostgreSQL -----------------------------------
// Devuelven el literal EXACTO que espera el CHECK correspondiente, acentos
// incluidos. NUNCA envolver estos valores en tr().

QString toDbString(AcquisitionType value);
QString toDbString(VehicleStatus value);
QString toDbString(InvoiceType value);
QString toDbString(PaymentType value);
QString toDbString(PaymentMethod value);
QString toDbString(Transmission value);
QString toDbString(WindowRegulators value);
QString toDbString(AirConditioning value);

// --- Lectura desde PostgreSQL -----------------------------------------
// nullopt = el texto de la base no pertenece al dominio (dato corrupto o
// escrito por una versión distinta del esquema). El llamador decide si eso
// es un error duro o un valor por omisión.

std::optional<AcquisitionType> acquisitionTypeFromDb(const QString &raw);
std::optional<VehicleStatus> vehicleStatusFromDb(const QString &raw);
std::optional<InvoiceType> invoiceTypeFromDb(const QString &raw);
std::optional<PaymentType> paymentTypeFromDb(const QString &raw);
std::optional<PaymentMethod> paymentMethodFromDb(const QString &raw);
std::optional<Transmission> transmissionFromDb(const QString &raw);
std::optional<WindowRegulators> windowRegulatorsFromDb(const QString &raw);
std::optional<AirConditioning> airConditioningFromDb(const QString &raw);

// --- Etiquetas para la interfaz ---------------------------------------
// Hoy coinciden con toDbString(), y esa duplicación es deliberada: separa lo
// que ve el usuario de lo que espera la base, para poder cambiar una sin
// romper la otra.

QString displayLabel(AcquisitionType value);
QString displayLabel(VehicleStatus value);
QString displayLabel(InvoiceType value);
QString displayLabel(PaymentType value);
QString displayLabel(PaymentMethod value);
QString displayLabel(Transmission value);
QString displayLabel(WindowRegulators value);
QString displayLabel(AirConditioning value);

// --- Listas para poblar combos ----------------------------------------
// Una sola fuente de verdad, en vez de literales repetidos en cada vista.
// Para guardar un valor en el userData de un QComboBox: static_cast<int>(v)
// al escribir y static_cast<Enum>(data.toInt()) al leer.

const QList<AcquisitionType> &allAcquisitionTypes();
const QList<VehicleStatus> &allVehicleStatuses();
const QList<PaymentType> &allPaymentTypes();
const QList<PaymentMethod> &allPaymentMethods();
const QList<Transmission> &allTransmissions();
const QList<WindowRegulators> &allWindowRegulators();
const QList<AirConditioning> &allAirConditioningModes();

// Los tipos de factura válidos para cada rama. Es lo que debe poblar el
// combo de factura del Paso 1, y tiene que repoblarse al cambiar el tipo de
// operación: si el combo se queda con los valores de adquisición y el
// usuario elige consignación, el INSERT viola el CHECK de la subtabla.
const QList<InvoiceType> &invoiceTypesFor(AcquisitionType type);

} // namespace domain

#endif // DOMAIN_ENUMS_H
