#include "domain/value_objects/enums.h"

namespace domain {
namespace {

// Busca el enum cuyo toDbString() coincide con `raw`. Recorrer la lista es
// más barato de mantener que una segunda tabla de texto->enum, que se
// desincronizaría en cuanto alguien corrigiera un acento en un solo lado.
template <typename Enum>
std::optional<Enum> fromDbString(const QList<Enum> &values, const QString &raw)
{
    for (Enum value : values) {
        if (toDbString(value) == raw)
            return value;
    }
    return std::nullopt;
}

} // namespace

// ---------------------------------------------------------------------------
// toDbString: literales EXACTOS de los CHECK de init-db/00_schema.sql.
// ---------------------------------------------------------------------------

QString toDbString(AcquisitionType value)
{
    switch (value) {
    case AcquisitionType::Adquisicion:
        return QStringLiteral("Adquisición");
    case AcquisitionType::Consignacion:
        return QStringLiteral("Consignación");
    }
    return QString();
}

QString toDbString(VehicleStatus value)
{
    switch (value) {
    case VehicleStatus::Disponible:
        return QStringLiteral("Disponible");
    case VehicleStatus::Apartado:
        return QStringLiteral("Apartado");
    case VehicleStatus::Vendido:
        return QStringLiteral("Vendido");
    }
    return QString();
}

QString toDbString(InvoiceType value)
{
    switch (value) {
    case InvoiceType::Facturado:
        return QStringLiteral("Facturado");
    case InvoiceType::Autofactura:
        return QStringLiteral("Autofactura");
    case InvoiceType::FacturadoReal:
        return QStringLiteral("Facturado REAL");
    case InvoiceType::NoFacturado:
        return QStringLiteral("No Facturado");
    }
    return QString();
}

QString toDbString(PaymentType value)
{
    switch (value) {
    case PaymentType::Contado:
        return QStringLiteral("Contado");
    case PaymentType::Credito:
        return QStringLiteral("Crédito");
    }
    return QString();
}

QString toDbString(PaymentMethod value)
{
    switch (value) {
    case PaymentMethod::Efectivo:
        return QStringLiteral("Efectivo");
    case PaymentMethod::Transferencia:
        return QStringLiteral("Transferencia");
    }
    return QString();
}

QString toDbString(Transmission value)
{
    switch (value) {
    case Transmission::Automatica:
        return QStringLiteral("Automático");
    case Transmission::Manual:
        return QStringLiteral("Manual");
    }
    return QString();
}

QString toDbString(WindowRegulators value)
{
    switch (value) {
    case WindowRegulators::Manuales:
        return QStringLiteral("Manuales");
    case WindowRegulators::ElectricosTradicionales:
        return QStringLiteral("Eléctricos tradicionales");
    case WindowRegulators::ElectricosInteligentes:
        return QStringLiteral("Eléctricos inteligentes");
    }
    return QString();
}

QString toDbString(AirConditioning value)
{
    switch (value) {
    case AirConditioning::Automatico:
        return QStringLiteral("Automático");
    case AirConditioning::Manual:
        return QStringLiteral("Manual");
    }
    return QString();
}

// ---------------------------------------------------------------------------
// Lectura desde la base
// ---------------------------------------------------------------------------

std::optional<AcquisitionType> acquisitionTypeFromDb(const QString &raw)
{
    return fromDbString(allAcquisitionTypes(), raw);
}

std::optional<VehicleStatus> vehicleStatusFromDb(const QString &raw)
{
    return fromDbString(allVehicleStatuses(), raw);
}

std::optional<InvoiceType> invoiceTypeFromDb(const QString &raw)
{
    static const QList<InvoiceType> all = {
        InvoiceType::Facturado,
        InvoiceType::Autofactura,
        InvoiceType::FacturadoReal,
        InvoiceType::NoFacturado,
    };
    return fromDbString(all, raw);
}

std::optional<PaymentType> paymentTypeFromDb(const QString &raw)
{
    return fromDbString(allPaymentTypes(), raw);
}

std::optional<PaymentMethod> paymentMethodFromDb(const QString &raw)
{
    return fromDbString(allPaymentMethods(), raw);
}

std::optional<Transmission> transmissionFromDb(const QString &raw)
{
    return fromDbString(allTransmissions(), raw);
}

std::optional<WindowRegulators> windowRegulatorsFromDb(const QString &raw)
{
    return fromDbString(allWindowRegulators(), raw);
}

std::optional<AirConditioning> airConditioningFromDb(const QString &raw)
{
    return fromDbString(allAirConditioningModes(), raw);
}

// ---------------------------------------------------------------------------
// Etiquetas de interfaz. Coinciden hoy con toDbString() a propósito -- ver el
// comentario del header sobre por qué son funciones separadas.
// ---------------------------------------------------------------------------

QString displayLabel(AcquisitionType value)
{
    return toDbString(value);
}

QString displayLabel(VehicleStatus value)
{
    return toDbString(value);
}

QString displayLabel(InvoiceType value)
{
    return toDbString(value);
}

QString displayLabel(PaymentType value)
{
    return toDbString(value);
}

QString displayLabel(PaymentMethod value)
{
    return toDbString(value);
}

QString displayLabel(Transmission value)
{
    return toDbString(value);
}

QString displayLabel(WindowRegulators value)
{
    return toDbString(value);
}

QString displayLabel(AirConditioning value)
{
    return toDbString(value);
}

// ---------------------------------------------------------------------------
// Listas para combos
// ---------------------------------------------------------------------------

const QList<AcquisitionType> &allAcquisitionTypes()
{
    static const QList<AcquisitionType> values = {
        AcquisitionType::Adquisicion,
        AcquisitionType::Consignacion,
    };
    return values;
}

const QList<VehicleStatus> &allVehicleStatuses()
{
    static const QList<VehicleStatus> values = {
        VehicleStatus::Disponible,
        VehicleStatus::Apartado,
        VehicleStatus::Vendido,
    };
    return values;
}

const QList<PaymentType> &allPaymentTypes()
{
    static const QList<PaymentType> values = {
        PaymentType::Contado,
        PaymentType::Credito,
    };
    return values;
}

const QList<PaymentMethod> &allPaymentMethods()
{
    static const QList<PaymentMethod> values = {
        PaymentMethod::Efectivo,
        PaymentMethod::Transferencia,
    };
    return values;
}

const QList<Transmission> &allTransmissions()
{
    static const QList<Transmission> values = {
        Transmission::Automatica,
        Transmission::Manual,
    };
    return values;
}

const QList<WindowRegulators> &allWindowRegulators()
{
    static const QList<WindowRegulators> values = {
        WindowRegulators::Manuales,
        WindowRegulators::ElectricosTradicionales,
        WindowRegulators::ElectricosInteligentes,
    };
    return values;
}

const QList<AirConditioning> &allAirConditioningModes()
{
    static const QList<AirConditioning> values = {
        AirConditioning::Automatico,
        AirConditioning::Manual,
    };
    return values;
}

const QList<InvoiceType> &invoiceTypesFor(AcquisitionType type)
{
    static const QList<InvoiceType> acquisitionTypes = {
        InvoiceType::Facturado,
        InvoiceType::Autofactura,
    };
    static const QList<InvoiceType> consignmentTypes = {
        InvoiceType::FacturadoReal,
        InvoiceType::NoFacturado,
    };
    return type == AcquisitionType::Adquisicion ? acquisitionTypes : consignmentTypes;
}

} // namespace domain
