#ifndef PAYMENTDTO_H
#define PAYMENTDTO_H

#include <QDate>
#include <QString>

// Una cuota de sale_payments. El alta masiva al vender a crédito la hace el
// servicio de venta; este DTO cubre cobro / consulta de una mensualidad.
struct PaymentDTO
{
    int id = 0;
    int saleFolio = 0;
    int installmentNumber = 0;
    QDate scheduledDate;
    double expectedAmount = 0.0;
    QDate paidDate;
    double paidAmount = 0.0;
    double capitalAllocation = 0.0;
    double interestAllocation = 0.0;
    double remainingBalance = 0.0;
    double lateFee = 0.0;
    QString status = QStringLiteral("Pendiente");
    bool isWaived = false;
    QString paymentMethod;
    QString referenceNumber;
    QString notes;
};

#endif // PAYMENTDTO_H
