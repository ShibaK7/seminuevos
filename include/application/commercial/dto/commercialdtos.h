#ifndef APPLICATION_COMMERCIAL_DTO_COMMERCIALDTOS_H
#define APPLICATION_COMMERCIAL_DTO_COMMERCIALDTOS_H

#include "application/commercial/sales/dto/saledtos.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace application {

struct CommercialFilterDto
{
    std::optional<int> month;
    std::optional<int> year;
    QString paymentType;
    QString searchText;
    bool collectionsMode = false;
};

struct CommercialSaleDto
{
    int folio = -1;
    QDate saleDate;
    QString buyerName;
    QString model;
    int yearModel = 0;
    QString paymentType;
    double totalAmount = 0.0;
    QString status;
    double paidAmount = 0.0;
    double balanceDue = 0.0;
};

struct CommercialPaymentDto
{
    int folio = -1;
    int installmentNumber = 0;
    QString paymentMethod;
    QDate scheduledDate;
    QDate paidDate;
    double expectedAmount = 0.0;
    double paidAmount = 0.0;
    double capitalAllocation = 0.0;
    double interestAllocation = 0.0;
    double remainingBalance = 0.0;
    bool waived = false;
    QString notes;
};

struct CommercialSaleDetailsDto
{
    int folio = -1;
    QDate saleDate;
    QString buyerName;
    QString vehicleName;
    double totalAmount = 0.0;
    double downPayment = 0.0;
    double amountToFinance = 0.0;
    double paidAmount = 0.0;
    double capitalPaid = 0.0;
    double interestPaid = 0.0;
    double originalDueAmount = 0.0;
    double balanceDue = 0.0;
    int paidInstallments = 0;
    int expectedInstallments = 0;
    QList<CommercialPaymentDto> payments;
};

struct CommercialSaleRegistrationDetailsDto
{
    int folio = -1;
    SaleRegistrationDto registration;
    SaleVehicleOptionDto vehicle;
    SaleVendorOptionDto vendor;
};

struct CommercialResultDto
{
    QList<CommercialSaleDto> sales;
    QString errorMessage;
};

struct CommercialSaleDetailsResultDto
{
    CommercialSaleDetailsDto sale;
    QString errorMessage;
};

struct CommercialSaleRegistrationDetailsResultDto
{
    CommercialSaleRegistrationDetailsDto sale;
    QString errorMessage;
};

struct CommercialWriteResultDto
{
    bool ok = false;
    QString errorMessage;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_DTO_COMMERCIALDTOS_H
