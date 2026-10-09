#include "domain/inventory/model/consignment/consignedvehicle.h"

#include "domain/inventory/model/vehiclevisitor.h"

#include <cmath>

namespace domain {
namespace {

constexpr double kMaxCommissionRate = 100.0;

// La columna es NUMERIC(12,2), así que el valor que guarda PostgreSQL viene
// redondeado a centavos. Redondear aquí también evita que el importe que ve
// el usuario antes de guardar difiera del que queda en la base: con
// base=100000 y comisión=8.33, la aritmética de punto flotante da
// 108329.99999... mientras que la base guarda 108330.00.
//
// Corolario: nunca comparar estos importes con ==.
double roundToCents(double value)
{
    return std::round(value * 100.0) / 100.0;
}

} // namespace

AcquisitionType ConsignedVehicle::acquisitionType() const
{
    return AcquisitionType::Consignacion;
}

QString ConsignedVehicle::counterpartyRole() const
{
    return QStringLiteral("Propietario");
}

double ConsignedVehicle::salePrice() const
{
    return roundToCents(m_basePrice + (m_basePrice * (m_commissionRate / 100.0)));
}

double ConsignedVehicle::totalCost() const
{
    return maintenanceCost();
}

double ConsignedVehicle::expectedProfit() const
{
    return commissionAmount() - maintenanceCost();
}

bool ConsignedVehicle::acceptsInvoiceType(InvoiceType type) const
{
    return type == InvoiceType::FacturadoReal || type == InvoiceType::NoFacturado;
}

void ConsignedVehicle::accept(VehicleVisitor &visitor) const
{
    visitor.visit(*this);
}

double ConsignedVehicle::basePrice() const
{
    return m_basePrice;
}

bool ConsignedVehicle::setBasePrice(double value)
{
    if (value <= 0.0)
        return false;
    m_basePrice = value;
    return true;
}

double ConsignedVehicle::commissionRate() const
{
    return m_commissionRate;
}

bool ConsignedVehicle::setCommissionRate(double value)
{
    // La columna admite hasta 999.99 por ser NUMERIC(5,2), pero una comisión
    // mayor al 100% no tiene sentido de negocio.
    if (value < 0.0 || value > kMaxCommissionRate)
        return false;
    m_commissionRate = value;
    return true;
}

double ConsignedVehicle::commissionAmount() const
{
    return roundToCents(m_basePrice * (m_commissionRate / 100.0));
}

double ConsignedVehicle::ownerPayout() const
{
    return m_basePrice;
}

bool ConsignedVehicle::canGenerateContract() const
{
    return true;
}

QString ConsignedVehicle::contractTitle() const
{
    return QStringLiteral("CONSIGNACIÓN AUTOMOTRIZ");
}

double ConsignedVehicle::contractAmount() const
{
    return m_basePrice;
}

QMap<QString, QString> ConsignedVehicle::contractPlaceholders() const
{
    QMap<QString, QString> values = Vehicle::contractPlaceholders();

    // La plantilla tiene un renglón de condiciones de pago que en la compra
    // describe si fue de contado o a crédito. Aquí se llena con el dato
    // equivalente de esta rama -- la comisión pactada -- para no dejar la
    // marca sin sustituir en el documento.
    values.insert(QStringLiteral("condiciones_pago"),
                  QStringLiteral("VENTA EN CONSIGNACIÓN. COMISIÓN PACTADA DEL %1%.")
                      .arg(m_commissionRate, 0, 'f', 2));

    return values;
}

void ConsignedVehicle::collectSpecificErrors(ValidationResult &result) const
{
    if (m_basePrice <= 0.0) {
        result.addError(QStringLiteral("basePrice"),
                        QStringLiteral("Captura el precio base que pide el propietario."));
    }
    // Una comisión de cero es rara pero puede ser una cortesía deliberada,
    // así que no se bloquea.
}

} // namespace domain
