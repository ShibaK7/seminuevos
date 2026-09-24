#ifndef CONSIGNATIONDTO_H
#define CONSIGNATIONDTO_H

#include "dto/counterpartydto.h"
#include "dto/vehicledto.h"

#include <QDate>
#include <QString>

// Wizard de registro cuando acquisition_type = 'Consignación'.
// sale_price lo calcula Postgres (GENERATED ALWAYS); no enviarlo en el INSERT.
struct ConsignationDTO
{
    VehicleDTO vehicle;

    int ownerId = 0;
    CounterpartyDTO owner;

    QString invoiceType;
    QString invoiceNumber;
    QString invoiceIssuer;
    double basePrice = 0.0;
    double commissionRate = 0.0;
    double maintenanceCost = 0.0;
    double salePrice = 0.0;
    QString observations;
    QDate consignmentDate;
};

#endif // CONSIGNATIONDTO_H
