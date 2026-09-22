#ifndef SALEDTO_H
#define SALEDTO_H

#include "dto/counterpartydto.h"

#include <QDate>
#include <QString>

// Condiciones de financiamiento. El servicio genera el calendario en
// sale_payments; este struct no lleva las cuotas individuales.
struct FinancingDTO
{
    double downPayment = 0.0;
    double amountToFinance = 0.0;
    double interestRate = 0.0;
    int monthsTerm = 0;
    double monthlyPayment = 0.0;
    double finalCalculatedPrice = 0.0;
    int avalId = 0;
    CounterpartyDTO aval;
    QString status = QStringLiteral("Al Corriente");
};

// Flujo de venta (sales + sale_financing opcional). paymentType 'Crédito'
// implica hasFinancing = true y un FinancingDTO válido.
struct SaleDTO
{
    int folio = 0;
    int vehicleFolio = 0;
    int vendorId = 0;

    int buyerId = 0;
    CounterpartyDTO buyer;

    QString paymentType;
    QString paymentMethod;
    double subtotal = 0.0;
    double iva = 16.0;
    double totalAmount = 0.0;
    QString observations;
    QString status;
    QDate saleDate;

    bool hasFinancing = false;
    FinancingDTO financing;
};

#endif // SALEDTO_H
