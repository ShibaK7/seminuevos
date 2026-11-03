#ifndef APPLICATION_COMMERCIAL_SALES_DTO_SALEDTOS_H
#define APPLICATION_COMMERCIAL_SALES_DTO_SALEDTOS_H

#include "application/common/dto/counterpartydtos.h"
#include "domain/common/value_objects/enums.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace application {

struct SaleBuyerDto
{
    int counterpartyId = -1;
    QString fullName;
    QString nationalId;
    QString streetAddress;
    QString suburb;
    QString locality;
    QString state;
    QString postalCode;
    QString phone;
    QString email;
};

struct SaleVehicleOptionDto
{
    int folio = -1;
    QString brandName;
    QString model;
    int yearModel = 0;
    double salePrice = 0.0;
    QString serialNumber;
    QString motorNumber;
    QString plates;
    QString color;
};

struct SaleVendorOptionDto
{
    int id = -1;
    QString displayName;
    QString username;
};

// A projected installment supplied by a future financing policy. This layer
// deliberately does not derive amounts or dates from the interest rate/term.
struct SaleInstallmentDto
{
    int number = 0;
    QDate dueDate;
    double expectedAmount = 0.0;
    double capitalAllocation = 0.0;
    double interestAllocation = 0.0;
    double remainingBalance = 0.0;
};

// Values are explicit snapshots from the sale form or a later financing
// calculator. No calculation policy is implied by this transport structure.
struct SaleFinancingDto
{
    double downPayment = 0.0;
    double amountToFinance = 0.0;
    double interestRatePercent = 0.0;
    int monthsTerm = 0;
    double monthlyPayment = 0.0;
    double finalPrice = 0.0;
    std::optional<SaleBuyerDto> guarantor;
    QList<SaleInstallmentDto> installments;
};

struct SaleRegistrationDto
{
    int vehicleFolio = -1;
    int vendorId = -1;
    QDate saleDate;
    SaleBuyerDto buyer;
    domain::PaymentType paymentType = domain::PaymentType::Contado;
    std::optional<domain::PaymentMethod> paymentMethod;
    double subtotal = 0.0;
    double iva = 0.0;
    double totalAmount = 0.0;
    QString observations;
    bool updateBuyerRecord = false;
    bool updateGuarantorRecord = false;
    std::optional<SaleFinancingDto> financing;
};

struct SaleSearchResultDto
{
    QList<SaleVehicleOptionDto> vehicles;
    QString errorMessage;
};

struct SaleVendorSearchResultDto
{
    QList<SaleVendorOptionDto> vendors;
    QString errorMessage;
};

using SaleCounterpartySearchResultDto = CounterpartySearchResultDto;

struct SaleRegistrationResultDto
{
    enum class Status { Registered, Rejected, Failed };
    Status status = Status::Failed;
    int saleFolio = -1;
    QString field;
    QString message;
};

} // namespace application

#endif // APPLICATION_COMMERCIAL_SALES_DTO_SALEDTOS_H
