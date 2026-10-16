#ifndef TESTS_SUPPORT_REGISTRATIONFIXTURES_H
#define TESTS_SUPPORT_REGISTRATIONFIXTURES_H

// Una captura completa y válida del asistente, para las pruebas del servicio y
// del presenter. Los ids coinciden con FakeReferenceDataReader.

#include "application/inventory/registration/dto/recorddtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"

#include <QDate>

namespace fixtures {

inline application::VehicleRegistrationDto validRegistration(domain::AcquisitionType type)
{
    application::VehicleRegistrationDto dto;
    application::VehicleDetailsDto &d = dto.details;
    d.acquisitionType = type;
    d.dealDate = QDate::currentDate();
    d.vehicleType = {1, QStringLiteral("Automóvil")};
    d.subtype = {5, QStringLiteral("Sedán")};
    d.brand = {3, QStringLiteral("Nissan")};
    d.model = QStringLiteral("Versa");
    d.yearModel = 2020;
    d.motorNumber = QStringLiteral("HR16");
    d.serialNumber = QStringLiteral("3N1CN7AD0LL000001");
    d.repuve = QStringLiteral("REP1");
    d.plates = QStringLiteral("ABC1234");
    d.platesHolder = QStringLiteral("Juan Pérez");
    d.counterparty.fullName = QStringLiteral("Juan Pérez");
    d.counterparty.nationalId = QStringLiteral("PEPJ800101");
    d.invoiceNumber = QStringLiteral("F-1");
    if (type == domain::AcquisitionType::Adquisicion) {
        d.invoiceType = domain::InvoiceType::Facturado;
        d.purchasePrice = 150000.0;
        d.salePrice = 180000.0;
        d.paymentMethod = domain::PaymentMethod::Transferencia;
    } else {
        d.invoiceType = domain::InvoiceType::FacturadoReal;
        d.basePrice = 200000.0;
        d.commissionRate = 10.0;
    }

    application::VehicleConditionsDto &c = dto.conditions;
    c.fuelType = {1, QStringLiteral("Gasolina")};
    c.cylinders = 4;
    c.transmission = domain::Transmission::Manual;
    c.interiorMaterial = QStringLiteral("Tela");
    c.windowRegulators = domain::WindowRegulators::Manuales;
    c.airConditioning = domain::AirConditioning::Manual;

    dto.files.images << domain::VehicleImage{QStringLiteral("foto1.jpg"), true};
    dto.files.documents << domain::VehicleDocument{QStringLiteral("Tarjeta de circulación"),
                                                   QStringLiteral("tarjeta.pdf"), QString()};
    return dto;
}

// La misma captura ya guardada con un folio, como la entrega el lector del
// expediente: rutas relativas al almacén, dos fotos (la portada es la
// segunda), un documento y, en una compra, la factura.
inline application::VehicleRecordDto storedRecord(int folio, domain::AcquisitionType type)
{
    const application::VehicleRegistrationDto captured = validRegistration(type);
    application::VehicleRecordDto record;
    record.folio = folio;
    record.version = QStringLiteral("1001");
    record.hasDealTerms = true;
    record.details = captured.details;
    record.details.dealDate = QDate(2024, 3, 15);
    record.details.maintenanceCost = 2500.0;
    if (type == domain::AcquisitionType::Adquisicion)
        record.details.invoiceFilePath = QStringLiteral("vehicles/VIN1/documents/factura.pdf");
    record.conditions = captured.conditions;
    record.conditions.inspection = {{7, false, QStringLiteral("Ponchada")}};
    record.images = {{{QStringLiteral("vehicles/VIN1/images/1.jpg"), false}, {}},
                     {{QStringLiteral("vehicles/VIN1/images/2.jpg"), true}, {}}};
    record.documents = {{QStringLiteral("Tarjeta de circulación"),
                         QStringLiteral("vehicles/VIN1/documents/tarjeta.pdf"), QString()}};
    return record;
}

} // namespace fixtures

#endif // TESTS_SUPPORT_REGISTRATIONFIXTURES_H
