#include "domain/consignedvehicle.h"

#include "domain/vehiclevisitor.h"

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

std::unique_ptr<Vehicle> ConsignedVehicle::clone() const
{
    return std::make_unique<ConsignedVehicle>(*this);
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
