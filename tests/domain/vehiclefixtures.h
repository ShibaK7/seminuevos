#ifndef TESTS_DOMAIN_VEHICLEFIXTURES_H
#define TESTS_DOMAIN_VEHICLEFIXTURES_H

// Datos de prueba compartidos: un borrador de compra y uno de consignación que
// pasan todas las reglas del dominio. Cada prueba parte de uno de ellos y
// rompe UN dato, así lo que se comprueba es esa regla y no el resto.

#include "domain/model/counterparty.h"
#include "domain/model/vehiclebuilder.h"
#include "domain/model/vehicleconditions.h"

#include <QDate>

namespace fixtures {

// UMA 2026 (la que siembra init-db). Tope de efectivo: 376,565.10.
inline constexpr double kUma2026 = 117.31;

inline domain::Counterparty validOwner()
{
    domain::Counterparty owner;
    (void)owner.setFullName(QStringLiteral("Juan Pérez López"));
    (void)owner.setNationalId(QStringLiteral("PELJ800101HDFRPN09"));
    return owner;
}

// Lo común a las dos ramas. El tipo de factura no va aquí porque cada rama
// acepta un conjunto distinto.
inline void fillCommonData(domain::VehicleBuilder &builder)
{
    builder.setDealDate(QDate::currentDate())
        .setVehicleType({1, QStringLiteral("Automóvil")})
        .setSubtype({5, QStringLiteral("Sedán")})
        .setBrand({3, QStringLiteral("Nissan")})
        .setModel(QStringLiteral("Versa Advance"))
        .setYearModel(2020)
        .setMileage(50000)
        .setSerialNumber(QStringLiteral("3N1CN7AD0LL000001"))
        .setMotorNumber(QStringLiteral("HR16123456"))
        .setPlates(QStringLiteral("ABC1234"))
        .setPlatesHolder(QStringLiteral("Juan Pérez López"))
        .setRepuve(QStringLiteral("REP1234567"))
        .setCounterparty(validOwner())
        .setInvoiceNumber(QStringLiteral("F-0001"));
}

// Compra válida pagada por transferencia (no la toca el tope de efectivo).
inline domain::VehicleBuilder validAcquisition(double uma = kUma2026)
{
    domain::VehicleBuilder builder;
    builder.setAcquisitionType(domain::AcquisitionType::Adquisicion);
    fillCommonData(builder);
    builder.setInvoiceType(domain::InvoiceType::Facturado)
        .setPurchasePrice(150000.0)
        .setSalePrice(180000.0)
        .setPaymentType(domain::PaymentType::Contado)
        .setPaymentMethod(domain::PaymentMethod::Transferencia)
        .setUmaDailyValue(uma);
    return builder;
}

inline domain::VehicleBuilder validConsignment()
{
    domain::VehicleBuilder builder;
    builder.setAcquisitionType(domain::AcquisitionType::Consignacion);
    fillCommonData(builder);
    builder.setInvoiceType(domain::InvoiceType::FacturadoReal)
        .setBasePrice(200000.0)
        .setCommissionRate(10.0);
    return builder;
}

inline domain::VehicleConditions validConditions()
{
    domain::VehicleConditions conditions;
    conditions.setFuelType({1, QStringLiteral("Gasolina")});
    (void)conditions.setCylinders(4);
    conditions.setTransmission(domain::Transmission::Manual);
    (void)conditions.setInteriorMaterial(QStringLiteral("Tela"));
    conditions.setWindowRegulators(domain::WindowRegulators::ElectricosTradicionales);
    conditions.setAirConditioning(domain::AirConditioning::Manual);
    return conditions;
}

} // namespace fixtures

#endif // TESTS_DOMAIN_VEHICLEFIXTURES_H
