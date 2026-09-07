#include "domain/vehiclebuilder.h"

namespace domain {

VehicleBuilder::VehicleBuilder()
{
    instantiate(AcquisitionType::Adquisicion);
}

VehicleBuilder::~VehicleBuilder() = default;
VehicleBuilder::VehicleBuilder(VehicleBuilder &&) noexcept = default;
VehicleBuilder &VehicleBuilder::operator=(VehicleBuilder &&) noexcept = default;

void VehicleBuilder::instantiate(AcquisitionType type)
{
    m_type = type;
    m_acquired = nullptr;
    m_consigned = nullptr;

    if (type == AcquisitionType::Adquisicion) {
        auto vehicle = std::make_unique<AcquiredVehicle>();
        m_acquired = vehicle.get();
        m_vehicle = std::move(vehicle);
    } else {
        auto vehicle = std::make_unique<ConsignedVehicle>();
        m_consigned = vehicle.get();
        m_vehicle = std::move(vehicle);
    }
}

void VehicleBuilder::noteRejected(const QString &field, const QString &message)
{
    m_rejected.addError(field, message);
}

VehicleBuilder &VehicleBuilder::setAcquisitionType(AcquisitionType type)
{
    // Cambiar de rama reinicia la captura a propósito: los datos propios de
    // una no tienen equivalente en la otra (un precio de compra no es un
    // precio base), y arrastrarlos produciría mezclas sin sentido. En la
    // práctica no se pierde nada, porque el asistente vuelve a volcar todos
    // los widgets sobre un builder nuevo en cada intento de guardado.
    if (!m_vehicle || m_type != type) {
        instantiate(type);
        m_rejected = ValidationResult();
    }
    return *this;
}

AcquisitionType VehicleBuilder::acquisitionType() const
{
    return m_type;
}

// ---------------------------------------------------------------------------
// Paso 1: datos comunes
// ---------------------------------------------------------------------------

VehicleBuilder &VehicleBuilder::setDealDate(QDate value)
{
    if (!m_vehicle->setDealDate(value)) {
        noteRejected(QStringLiteral("dealDate"),
                     QStringLiteral("La fecha de la operación no puede estar en el futuro."));
    } else {
        // El alta en inventario sigue a la fecha del trato salvo que alguien
        // la fije aparte.
        m_vehicle->setAddedDate(value);
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setVehicleType(const CatalogRef &value)
{
    m_vehicle->setVehicleType(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setSubtype(const CatalogRef &value)
{
    m_vehicle->setSubtype(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setBrand(const CatalogRef &value)
{
    m_vehicle->setBrand(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setModel(const QString &value)
{
    if (!m_vehicle->setModel(value)) {
        noteRejected(QStringLiteral("model"),
                     QStringLiteral("El modelo no puede pasar de 50 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setYearModel(int value)
{
    if (!m_vehicle->setYearModel(value)) {
        noteRejected(QStringLiteral("yearModel"),
                     QStringLiteral("El año del modelo (%1) está fuera de rango.").arg(value));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setColor(const QString &value)
{
    if (!m_vehicle->setColor(value)) {
        noteRejected(QStringLiteral("color"),
                     QStringLiteral("El color no puede pasar de 30 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setMileage(int value)
{
    if (!m_vehicle->setMileage(value)) {
        noteRejected(QStringLiteral("mileage"),
                     QStringLiteral("El kilometraje (%1) no es válido.").arg(value));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setSerialNumber(const QString &value)
{
    if (!m_vehicle->setSerialNumber(value)) {
        noteRejected(QStringLiteral("serialNumber"),
                     QStringLiteral("El número de serie no puede pasar de 50 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setMotorNumber(const QString &value)
{
    if (!m_vehicle->setMotorNumber(value)) {
        noteRejected(QStringLiteral("motorNumber"),
                     QStringLiteral("El número de motor no puede pasar de 50 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setPlates(const QString &value)
{
    if (!m_vehicle->setPlates(value)) {
        noteRejected(QStringLiteral("plates"),
                     QStringLiteral("Las placas no pueden pasar de 20 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setPlatesHolder(const QString &value)
{
    if (!m_vehicle->setPlatesHolder(value)) {
        noteRejected(QStringLiteral("platesHolder"),
                     QStringLiteral("El titular de las placas no puede pasar de 100 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setRepuve(const QString &value)
{
    if (!m_vehicle->setRepuve(value)) {
        noteRejected(QStringLiteral("repuve"),
                     QStringLiteral("El REPUVE no puede pasar de 50 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setDescription(const QString &value)
{
    m_vehicle->setDescription(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setCounterparty(const Counterparty &value)
{
    m_vehicle->setCounterparty(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setInvoiceType(InvoiceType value)
{
    // El rechazo aquí significa que el tipo no pertenece a esta rama, cosa
    // que el asistente evita repoblando el combo al cambiar de operación.
    // Si aun así llega, validate() lo vuelve a reportar.
    if (!m_vehicle->setInvoiceType(value)) {
        noteRejected(QStringLiteral("invoiceType"),
                     QStringLiteral("El tipo de factura no corresponde a este tipo de operación."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setInvoiceNumber(const QString &value)
{
    if (!m_vehicle->setInvoiceNumber(value)) {
        noteRejected(QStringLiteral("invoiceNumber"),
                     QStringLiteral("El número de factura no puede pasar de 50 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setInvoiceIssuer(const QString &value)
{
    if (!m_vehicle->setInvoiceIssuer(value)) {
        noteRejected(QStringLiteral("invoiceIssuer"),
                     QStringLiteral("El emisor de la factura no puede pasar de 150 caracteres."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setMaintenanceCost(double value)
{
    if (!m_vehicle->setMaintenanceCost(value)) {
        noteRejected(QStringLiteral("maintenanceCost"),
                     QStringLiteral("El costo de mantenimiento no puede ser negativo."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setObservations(const QString &value)
{
    m_vehicle->setObservations(value);
    return *this;
}

// ---------------------------------------------------------------------------
// Paso 1: solo compra
// ---------------------------------------------------------------------------

VehicleBuilder &VehicleBuilder::setPurchasePrice(double value)
{
    if (m_acquired && !m_acquired->setPurchasePrice(value)) {
        noteRejected(QStringLiteral("purchasePrice"),
                     QStringLiteral("El precio de compra debe ser mayor a cero."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setSalePrice(double value)
{
    if (m_acquired && !m_acquired->setSalePrice(value)) {
        noteRejected(QStringLiteral("salePrice"),
                     QStringLiteral("El precio de venta no puede ser negativo."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setPaymentType(PaymentType value)
{
    if (m_acquired)
        m_acquired->setPaymentType(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setPaymentMethod(PaymentMethod value)
{
    if (m_acquired)
        m_acquired->setPaymentMethod(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setInvoiceFilePath(const QString &value)
{
    if (m_acquired)
        m_acquired->setInvoiceFilePath(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setUmaDailyValue(double value)
{
    if (m_acquired && !m_acquired->setUmaDailyValue(value)) {
        noteRejected(QStringLiteral("umaDailyValue"),
                     QStringLiteral("El valor de la UMA no puede ser negativo."));
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Paso 1: solo consignación
// ---------------------------------------------------------------------------

VehicleBuilder &VehicleBuilder::setBasePrice(double value)
{
    if (m_consigned && !m_consigned->setBasePrice(value)) {
        noteRejected(QStringLiteral("basePrice"),
                     QStringLiteral("El precio base debe ser mayor a cero."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::setCommissionRate(double value)
{
    if (m_consigned && !m_consigned->setCommissionRate(value)) {
        noteRejected(QStringLiteral("commissionRate"),
                     QStringLiteral("La comisión debe estar entre 0 y 100 por ciento."));
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Pasos 2 y 3
// ---------------------------------------------------------------------------

VehicleBuilder &VehicleBuilder::setConditions(const VehicleConditions &value)
{
    m_vehicle->setConditions(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::setInspection(const Inspection &value)
{
    m_vehicle->setInspection(value);
    return *this;
}

VehicleBuilder &VehicleBuilder::addImage(const VehicleImage &value)
{
    if (!m_vehicle->addImage(value)) {
        noteRejected(QStringLiteral("images"),
                     QStringLiteral("No se pudo agregar la fotografía: solo puede haber una "
                                    "imagen de portada."));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::addDocument(const VehicleDocument &value)
{
    if (!m_vehicle->addDocument(value)) {
        noteRejected(QStringLiteral("documents"),
                     QStringLiteral("Hay más de un documento del tipo '%1'.")
                         .arg(value.documentType));
    }
    return *this;
}

VehicleBuilder &VehicleBuilder::clearFiles()
{
    m_vehicle->clearImages();
    m_vehicle->clearDocuments();
    return *this;
}

// ---------------------------------------------------------------------------
// Validación y entrega
// ---------------------------------------------------------------------------

ValidationResult VehicleBuilder::validateVehicleData() const
{
    ValidationResult result = m_rejected;
    result.merge(m_vehicle->validate());
    return result;
}

ValidationResult VehicleBuilder::validateConditionData() const
{
    ValidationResult result;
    result.merge(m_vehicle->conditions().validate(), QStringLiteral("conditions"));
    result.merge(m_vehicle->inspection().validate(), QStringLiteral("inspection"));
    return result;
}

std::unique_ptr<Vehicle> VehicleBuilder::build(ValidationResult &result)
{
    result = validateVehicleData();
    result.merge(validateConditionData());

    if (!result.isValid())
        return nullptr;

    // El objeto sale del builder. Se deja uno nuevo en su lugar para que el
    // builder quede en un estado usable en vez de con un puntero nulo que
    // haría fallar cualquier setter posterior.
    std::unique_ptr<Vehicle> built = std::move(m_vehicle);
    instantiate(m_type);
    m_rejected = ValidationResult();
    return built;
}

} // namespace domain
