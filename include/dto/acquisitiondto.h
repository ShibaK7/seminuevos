#ifndef ACQUISITIONDTO_H
#define ACQUISITIONDTO_H

#include "dto/counterpartydto.h"
#include "dto/vehicledto.h"

#include <QDate>
#include <QString>

// Wizard de registro cuando acquisition_type = 'Adquisición'.
// El servicio (Rule 3) persiste vehicles + vehicle_acquisitions + conditions
// + inspection + archivos en una sola transacción. sellerId > 0 reutiliza
// una contraparte; si no, seller se inserta en el mismo flujo.
struct AcquisitionDTO
{
    VehicleDTO vehicle;

    int sellerId = 0;
    CounterpartyDTO seller;

    QString invoiceType;
    QString invoiceNumber;
    QString invoiceIssuer;
    QString invoiceFilePath;
    double purchasePrice = 0.0;
    QString paymentType;
    QString paymentMethod;
    double maintenanceCost = 0.0;
    double salePrice = 0.0;
    QString observations;
    QDate acquisitionDate;
};

#endif // ACQUISITIONDTO_H
