#include "adapters/persistence/commercial/sqlcommercial.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

enum Column {
    ColFolio = 0,
    ColSaleDate,
    ColBuyerName,
    ColModel,
    ColYearModel,
    ColPaymentType,
    ColTotalAmount,
    ColStatus,
    ColPaidAmount,
    ColBalanceDue,
};

enum DetailsColumn {
    DetailsFolio = 0,
    DetailsSaleDate,
    DetailsBuyerName,
    DetailsVehicleName,
    DetailsTotalAmount,
    DetailsDownPayment,
    DetailsAmountToFinance,
    DetailsPaidAmount,
    DetailsCapitalPaid,
    DetailsInterestPaid,
    DetailsBalanceDue,
    DetailsPaidInstallments,
    DetailsExpectedInstallments,
    DetailsOriginalDueAmount,
};

enum PaymentColumn {
    PaymentFolio = 0,
    PaymentInstallment,
    PaymentMethod,
    PaymentScheduledDate,
    PaymentPaidDate,
    PaymentExpectedAmount,
    PaymentAmount,
    PaymentCapital,
    PaymentInterest,
    PaymentRemainingBalance,
    PaymentWaived,
    PaymentNotes,
};

enum RegistrationDetailsColumn {
    RegistrationFolio = 0,
    RegistrationSaleDate,
    RegistrationPaymentType,
    RegistrationPaymentMethod,
    RegistrationSubtotal,
    RegistrationIva,
    RegistrationTotal,
    RegistrationObservations,
    RegistrationVehicleFolio,
    RegistrationVehicleBrand,
    RegistrationVehicleModel,
    RegistrationVehicleYear,
    RegistrationVehiclePrice,
    RegistrationVehicleSerial,
    RegistrationVehicleMotor,
    RegistrationVehiclePlates,
    RegistrationVehicleColor,
    RegistrationVendorName,
    RegistrationVendorUsername,
    RegistrationVendorId,
    RegistrationBuyerId,
    RegistrationBuyerName,
    RegistrationBuyerNationalId,
    RegistrationBuyerAddress,
    RegistrationBuyerSuburb,
    RegistrationBuyerLocality,
    RegistrationBuyerState,
    RegistrationBuyerPostalCode,
    RegistrationBuyerPhone,
    RegistrationBuyerEmail,
    RegistrationDownPayment,
    RegistrationAmountToFinance,
    RegistrationInterestRate,
    RegistrationMonthsTerm,
    RegistrationMonthlyPayment,
    RegistrationFinalPrice,
    RegistrationGuarantorId,
    RegistrationGuarantorName,
    RegistrationGuarantorNationalId,
    RegistrationGuarantorAddress,
    RegistrationGuarantorSuburb,
    RegistrationGuarantorLocality,
    RegistrationGuarantorState,
    RegistrationGuarantorPostalCode,
    RegistrationGuarantorPhone,
    RegistrationGuarantorEmail,
};

constexpr auto kSalesSelect = R"SQL(
SELECT s.folio,
       s.sale_date,
       buyer.full_name,
       v.model,
       v.year_model,
       s.payment_type,
       s.total_amount,
       s.status,
       CASE
           WHEN s.payment_type = 'Contado' AND s.status = 'Pago Completo'
               THEN s.total_amount
           ELSE COALESCE(fin.down_payment, 0) + COALESCE(payments.paid_amount, 0)
       END AS paid_amount,
       s.total_amount - CASE
           WHEN s.payment_type = 'Contado' AND s.status = 'Pago Completo'
               THEN s.total_amount
           ELSE COALESCE(fin.down_payment, 0) + COALESCE(payments.paid_amount, 0)
       END AS balance_due
  FROM sales s
  JOIN vehicles v ON v.folio = s.vehicle_folio
  JOIN counterparties buyer ON buyer.id = s.buyer_id
  LEFT JOIN sale_financing fin ON fin.sale_folio = s.folio
  LEFT JOIN LATERAL (
      SELECT SUM(payment.paid_amount) AS paid_amount
        FROM sale_payments payment
       WHERE payment.sale_folio = s.folio
  ) payments ON TRUE
)SQL";

constexpr auto kSaleDetailsSelect = R"SQL(
SELECT s.folio,
       s.sale_date,
       buyer.full_name,
       concat_ws(' ', brand.name, v.model) AS vehicle_name,
       COALESCE(fin.final_calculated_price, s.total_amount) AS total_amount,
       COALESCE(fin.down_payment, 0) AS down_payment,
       COALESCE(fin.amount_to_finance, 0) AS amount_to_finance,
       COALESCE(payments.paid_amount, 0) AS paid_amount,
       COALESCE(payments.capital_paid, 0) AS capital_paid,
       COALESCE(payments.interest_paid, 0) AS interest_paid,
       COALESCE(fin.final_calculated_price, s.total_amount)
           - COALESCE(fin.down_payment, 0)
           - COALESCE(payments.paid_amount, 0) AS balance_due,
       COALESCE(payments.paid_installments, 0) AS paid_installments,
       COALESCE(fin.months_term, 0) AS expected_installments,
       COALESCE(fin.final_calculated_price, s.total_amount)
           - COALESCE(fin.down_payment, 0) AS original_due_amount
  FROM sales s
  JOIN vehicles v ON v.folio = s.vehicle_folio
  LEFT JOIN brands_cat brand ON brand.id = v.brand_id
  JOIN counterparties buyer ON buyer.id = s.buyer_id
  LEFT JOIN sale_financing fin ON fin.sale_folio = s.folio
  LEFT JOIN LATERAL (
      SELECT SUM(payment.paid_amount) FILTER (
                 WHERE payment.paid_amount <> 0 AND payment.paid_date IS NOT NULL
             ) AS paid_amount,
             SUM(payment.capital_allocation) FILTER (
                 WHERE payment.paid_amount <> 0 AND payment.paid_date IS NOT NULL
             ) AS capital_paid,
             SUM(payment.interest_allocation) FILTER (
                 WHERE payment.paid_amount <> 0 AND payment.paid_date IS NOT NULL
             ) AS interest_paid,
             COUNT(*) FILTER (
                 WHERE payment.paid_amount <> 0 AND payment.paid_date IS NOT NULL
             ) AS paid_installments
        FROM sale_payments payment
       WHERE payment.sale_folio = s.folio
  ) payments ON TRUE
 WHERE s.folio = :folio
)SQL";

constexpr auto kSalePaymentsSelect = R"SQL(
SELECT id,
       installment_number,
       payment_method,
       scheduled_date,
       paid_date,
       expected_amount,
       paid_amount,
       capital_allocation,
       interest_allocation,
       remaining_balance,
       is_waived,
       notes
  FROM sale_payments
 WHERE sale_folio = :folio
 ORDER BY installment_number, id
)SQL";

constexpr auto kSaleRegistrationDetailsSelect = R"SQL(
SELECT s.folio, s.sale_date, s.payment_type, s.payment_method,
       s.subtotal, s.iva, s.total_amount, s.observations,
       v.folio, COALESCE(brand.name, ''), v.model, v.year_model,
       COALESCE(acq.sale_price, csg.sale_price, 0),
       v.serial_number, v.motor_number, v.plates, v.color,
       vendor.display_name, vendor.username, vendor.id,
       buyer.id, buyer.full_name, buyer.national_id, buyer.street_address,
       buyer.suburb, buyer.locality, buyer.state, buyer.postal_code,
       buyer.phone, buyer.email,
       fin.down_payment, fin.amount_to_finance, fin.interest_rate,
       fin.months_term, fin.monthly_payment, fin.final_calculated_price,
       guarantor.id, guarantor.full_name, guarantor.national_id,
       guarantor.street_address, guarantor.suburb, guarantor.locality,
       guarantor.state, guarantor.postal_code, guarantor.phone, guarantor.email
  FROM sales s
  JOIN vehicles v ON v.folio = s.vehicle_folio
  JOIN users vendor ON vendor.id = s.vendor_id
  JOIN counterparties buyer ON buyer.id = s.buyer_id
  LEFT JOIN brands_cat brand ON brand.id = v.brand_id
  LEFT JOIN vehicle_acquisitions acq ON acq.vehicle_folio = v.folio
  LEFT JOIN vehicle_consignments csg ON csg.vehicle_folio = v.folio
  LEFT JOIN sale_financing fin ON fin.sale_folio = s.folio
  LEFT JOIN counterparties guarantor ON guarantor.id = fin.aval_id
 WHERE s.folio = :folio
)SQL";

} // namespace

SqlCommercial::SqlCommercial(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::CommercialSaleDto> SqlCommercial::search(
    const application::CommercialFilterDto &filter, QString *error)
{
    QList<application::CommercialSaleDto> rows;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);

        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QString sql = QString::fromLatin1(kSalesSelect);
            QStringList clauses;
            if (filter.month)
                clauses << QStringLiteral("EXTRACT(MONTH FROM s.sale_date) = :month");
            if (filter.year)
                clauses << QStringLiteral("EXTRACT(YEAR FROM s.sale_date) = :year");
            if (!filter.collectionsMode && !filter.paymentType.isEmpty())
                clauses << QStringLiteral("s.payment_type = :payment_type");
            if (!filter.searchText.trimmed().isEmpty() && filter.collectionsMode)
                clauses << QStringLiteral("buyer.full_name ILIKE :search_buyer");
            else if (!filter.searchText.trimmed().isEmpty())
                clauses << QStringLiteral(
                    "(CAST(s.folio AS TEXT) ILIKE :search_folio "
                    "OR buyer.full_name ILIKE :search_buyer "
                    "OR v.model ILIKE :search_model)");

            if (filter.collectionsMode)
                clauses << QStringLiteral("s.status IN ('Por Cobrar', 'Atrasado')");

            if (!clauses.isEmpty())
                sql += QStringLiteral("\n WHERE ") + clauses.join(QStringLiteral("\n   AND "));
            sql += QStringLiteral("\n ORDER BY s.sale_date DESC, s.folio DESC");

            if (!query.prepare(sql)) {
                failure = query.lastError().text();
            } else {
                if (filter.month)
                    query.bindValue(QStringLiteral(":month"), *filter.month);
                if (filter.year)
                    query.bindValue(QStringLiteral(":year"), *filter.year);
                if (!filter.collectionsMode && !filter.paymentType.isEmpty())
                    query.bindValue(QStringLiteral(":payment_type"), filter.paymentType);
                if (!filter.searchText.trimmed().isEmpty()) {
                    const QString pattern = QLatin1Char('%') + filter.searchText.trimmed()
                                            + QLatin1Char('%');
                    query.bindValue(QStringLiteral(":search_buyer"), pattern);
                    if (!filter.collectionsMode) {
                        query.bindValue(QStringLiteral(":search_folio"), pattern);
                        query.bindValue(QStringLiteral(":search_model"), pattern);
                    }
                }

                if (!query.exec()) {
                    failure = query.lastError().text();
                } else {
                    while (query.next()) {
                        application::CommercialSaleDto sale;
                        sale.folio = query.value(ColFolio).toInt();
                        sale.saleDate = query.value(ColSaleDate).toDate();
                        sale.buyerName = query.value(ColBuyerName).toString();
                        sale.model = query.value(ColModel).toString();
                        sale.yearModel = query.value(ColYearModel).toInt();
                        sale.paymentType = query.value(ColPaymentType).toString();
                        sale.totalAmount = query.value(ColTotalAmount).toDouble();
                        sale.status = query.value(ColStatus).toString();
                        sale.paidAmount = query.value(ColPaidAmount).toDouble();
                        sale.balanceDue = query.value(ColBalanceDue).toDouble();
                        rows << sale;
                    }
                }
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    } else if (error) {
        error->clear();
    }
    return rows;
}

application::CommercialSaleDetailsDto SqlCommercial::saleDetails(int folio, QString *error)
{
    application::CommercialSaleDetailsDto sale;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery detailsQuery(db);
            if (!detailsQuery.prepare(QString::fromLatin1(kSaleDetailsSelect))) {
                failure = detailsQuery.lastError().text();
            } else {
                detailsQuery.bindValue(QStringLiteral(":folio"), folio);
                if (!detailsQuery.exec()) {
                    failure = detailsQuery.lastError().text();
                } else if (!detailsQuery.next()) {
                    failure = QStringLiteral("No existe una venta con folio %1.").arg(folio);
                } else {
                    sale.folio = detailsQuery.value(DetailsFolio).toInt();
                    sale.saleDate = detailsQuery.value(DetailsSaleDate).toDate();
                    sale.buyerName = detailsQuery.value(DetailsBuyerName).toString();
                    sale.vehicleName = detailsQuery.value(DetailsVehicleName).toString();
                    sale.totalAmount = detailsQuery.value(DetailsTotalAmount).toDouble();
                    sale.downPayment = detailsQuery.value(DetailsDownPayment).toDouble();
                    sale.amountToFinance = detailsQuery.value(DetailsAmountToFinance).toDouble();
                    sale.paidAmount = detailsQuery.value(DetailsPaidAmount).toDouble();
                    sale.capitalPaid = detailsQuery.value(DetailsCapitalPaid).toDouble();
                    sale.interestPaid = detailsQuery.value(DetailsInterestPaid).toDouble();
                    sale.balanceDue = detailsQuery.value(DetailsBalanceDue).toDouble();
                    sale.paidInstallments =
                        detailsQuery.value(DetailsPaidInstallments).toInt();
                    sale.expectedInstallments =
                        detailsQuery.value(DetailsExpectedInstallments).toInt();
                    sale.originalDueAmount =
                        detailsQuery.value(DetailsOriginalDueAmount).toDouble();

                    QSqlQuery paymentsQuery(db);
                    if (!paymentsQuery.prepare(QString::fromLatin1(kSalePaymentsSelect))) {
                        failure = paymentsQuery.lastError().text();
                    } else {
                        paymentsQuery.bindValue(QStringLiteral(":folio"), folio);
                        if (!paymentsQuery.exec())
                            failure = paymentsQuery.lastError().text();
                        while (failure.isEmpty() && paymentsQuery.next()) {
                            application::CommercialPaymentDto payment;
                            payment.folio = paymentsQuery.value(PaymentFolio).toInt();
                            payment.installmentNumber =
                                paymentsQuery.value(PaymentInstallment).toInt();
                            payment.paymentMethod = paymentsQuery.value(PaymentMethod).toString();
                            payment.scheduledDate =
                                paymentsQuery.value(PaymentScheduledDate).toDate();
                            payment.paidDate = paymentsQuery.value(PaymentPaidDate).toDate();
                            payment.expectedAmount =
                                paymentsQuery.value(PaymentExpectedAmount).toDouble();
                            payment.paidAmount = paymentsQuery.value(PaymentAmount).toDouble();
                            payment.capitalAllocation =
                                paymentsQuery.value(PaymentCapital).toDouble();
                            payment.interestAllocation =
                                paymentsQuery.value(PaymentInterest).toDouble();
                            payment.remainingBalance =
                                paymentsQuery.value(PaymentRemainingBalance).toDouble();
                            payment.waived = paymentsQuery.value(PaymentWaived).toBool();
                            payment.notes = paymentsQuery.value(PaymentNotes).toString();
                            sale.payments << payment;
                        }
                    }
                }
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    } else if (error) {
        error->clear();
    }
    return sale;
}

application::CommercialSaleRegistrationDetailsDto SqlCommercial::saleRegistrationDetails(
    int folio, QString *error)
{
    application::CommercialSaleRegistrationDetailsDto result;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery saleQuery(db);
            if (!saleQuery.prepare(QString::fromLatin1(kSaleRegistrationDetailsSelect))) {
                failure = saleQuery.lastError().text();
            } else {
                saleQuery.bindValue(QStringLiteral(":folio"), folio);
                if (!saleQuery.exec()) {
                    failure = saleQuery.lastError().text();
                } else if (!saleQuery.next()) {
                    failure = QStringLiteral("No existe una venta con folio %1.").arg(folio);
                } else {
                    const auto paymentType = domain::paymentTypeFromDb(
                        saleQuery.value(RegistrationPaymentType).toString());
                    if (!paymentType) {
                        failure = QStringLiteral("La venta %1 tiene un tipo de pago inválido.")
                                      .arg(folio);
                    } else {
                        result.folio = saleQuery.value(RegistrationFolio).toInt();
                        application::SaleRegistrationDto &registration = result.registration;
                        registration.saleDate = saleQuery.value(RegistrationSaleDate).toDate();
                        registration.paymentType = *paymentType;
                        registration.subtotal = saleQuery.value(RegistrationSubtotal).toDouble();
                        registration.iva = saleQuery.value(RegistrationIva).toDouble();
                        registration.totalAmount = saleQuery.value(RegistrationTotal).toDouble();
                        registration.observations =
                            saleQuery.value(RegistrationObservations).toString();

                        if (*paymentType == domain::PaymentType::Contado
                            && !saleQuery.value(RegistrationPaymentMethod).isNull()
                            && !saleQuery.value(RegistrationPaymentMethod).toString().isEmpty()) {
                            registration.paymentMethod = domain::paymentMethodFromDb(
                                saleQuery.value(RegistrationPaymentMethod).toString());
                            if (!registration.paymentMethod) {
                                failure = QStringLiteral(
                                              "La venta %1 tiene un método de pago inválido.")
                                              .arg(folio);
                            }
                        }

                        result.vehicle.folio = saleQuery.value(RegistrationVehicleFolio).toInt();
                        result.vehicle.brandName =
                            saleQuery.value(RegistrationVehicleBrand).toString();
                        result.vehicle.model = saleQuery.value(RegistrationVehicleModel).toString();
                        result.vehicle.yearModel =
                            saleQuery.value(RegistrationVehicleYear).toInt();
                        result.vehicle.salePrice =
                            saleQuery.value(RegistrationVehiclePrice).toDouble();
                        result.vehicle.serialNumber =
                            saleQuery.value(RegistrationVehicleSerial).toString();
                        result.vehicle.motorNumber =
                            saleQuery.value(RegistrationVehicleMotor).toString();
                        result.vehicle.plates =
                            saleQuery.value(RegistrationVehiclePlates).toString();
                        result.vehicle.color =
                            saleQuery.value(RegistrationVehicleColor).toString();

                        result.vendor.displayName =
                            saleQuery.value(RegistrationVendorName).toString();
                        result.vendor.username =
                            saleQuery.value(RegistrationVendorUsername).toString();
                        result.vendor.id = saleQuery.value(RegistrationVendorId).toInt();

                        registration.vehicleFolio = result.vehicle.folio;
                        registration.buyer.counterpartyId =
                            saleQuery.value(RegistrationBuyerId).toInt();
                        registration.buyer.fullName =
                            saleQuery.value(RegistrationBuyerName).toString();
                        registration.buyer.nationalId =
                            saleQuery.value(RegistrationBuyerNationalId).toString();
                        registration.buyer.streetAddress =
                            saleQuery.value(RegistrationBuyerAddress).toString();
                        registration.buyer.suburb =
                            saleQuery.value(RegistrationBuyerSuburb).toString();
                        registration.buyer.locality =
                            saleQuery.value(RegistrationBuyerLocality).toString();
                        registration.buyer.state =
                            saleQuery.value(RegistrationBuyerState).toString();
                        registration.buyer.postalCode =
                            saleQuery.value(RegistrationBuyerPostalCode).toString();
                        registration.buyer.phone =
                            saleQuery.value(RegistrationBuyerPhone).toString();
                        registration.buyer.email =
                            saleQuery.value(RegistrationBuyerEmail).toString();

                        if (!saleQuery.value(RegistrationDownPayment).isNull()) {
                            application::SaleFinancingDto financing;
                            financing.downPayment =
                                saleQuery.value(RegistrationDownPayment).toDouble();
                            financing.amountToFinance =
                                saleQuery.value(RegistrationAmountToFinance).toDouble();
                            financing.interestRatePercent =
                                saleQuery.value(RegistrationInterestRate).toDouble();
                            financing.monthsTerm =
                                saleQuery.value(RegistrationMonthsTerm).toInt();
                            financing.monthlyPayment =
                                saleQuery.value(RegistrationMonthlyPayment).toDouble();
                            financing.finalPrice =
                                saleQuery.value(RegistrationFinalPrice).toDouble();

                            if (!saleQuery.value(RegistrationGuarantorId).isNull()) {
                                application::SaleBuyerDto guarantor;
                                guarantor.counterpartyId =
                                    saleQuery.value(RegistrationGuarantorId).toInt();
                                guarantor.fullName =
                                    saleQuery.value(RegistrationGuarantorName).toString();
                                guarantor.nationalId =
                                    saleQuery.value(RegistrationGuarantorNationalId).toString();
                                guarantor.streetAddress =
                                    saleQuery.value(RegistrationGuarantorAddress).toString();
                                guarantor.suburb =
                                    saleQuery.value(RegistrationGuarantorSuburb).toString();
                                guarantor.locality =
                                    saleQuery.value(RegistrationGuarantorLocality).toString();
                                guarantor.state =
                                    saleQuery.value(RegistrationGuarantorState).toString();
                                guarantor.postalCode =
                                    saleQuery.value(RegistrationGuarantorPostalCode).toString();
                                guarantor.phone =
                                    saleQuery.value(RegistrationGuarantorPhone).toString();
                                guarantor.email =
                                    saleQuery.value(RegistrationGuarantorEmail).toString();
                                financing.guarantor = guarantor;
                            }

                            QSqlQuery installmentsQuery(db);
                            if (!installmentsQuery.prepare(QStringLiteral(
                                    "SELECT installment_number, scheduled_date, expected_amount, "
                                    "capital_allocation, interest_allocation, remaining_balance "
                                    "FROM sale_payments WHERE sale_folio = :folio "
                                    "ORDER BY installment_number, id"))) {
                                failure = installmentsQuery.lastError().text();
                            } else {
                                installmentsQuery.bindValue(QStringLiteral(":folio"), folio);
                                if (!installmentsQuery.exec()) {
                                    failure = installmentsQuery.lastError().text();
                                } else {
                                    while (installmentsQuery.next()) {
                                        application::SaleInstallmentDto installment;
                                        installment.number = installmentsQuery.value(0).toInt();
                                        installment.dueDate = installmentsQuery.value(1).toDate();
                                        installment.expectedAmount =
                                            installmentsQuery.value(2).toDouble();
                                        installment.capitalAllocation =
                                            installmentsQuery.value(3).toDouble();
                                        installment.interestAllocation =
                                            installmentsQuery.value(4).toDouble();
                                        installment.remainingBalance =
                                            installmentsQuery.value(5).toDouble();
                                        financing.installments << installment;
                                    }
                                }
                            }
                            registration.financing = financing;
                        }
                    }
                }
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    } else if (error) {
        error->clear();
    }
    return result;
}

bool SqlCommercial::updatePaymentWaiver(int paymentFolio, bool waived,
                                       domain::UserRole role, QString *error)
{
    if (role != domain::UserRole::Admin) {
        if (error)
            *error = QStringLiteral("Solo un Administrador puede modificar condonaciones.");
        return false;
    }

    QString failure;
    bool updated = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery query(db);
            if (!query.prepare(QStringLiteral(
                    "UPDATE sale_payments SET is_waived = :waived WHERE id = :payment_folio"))) {
                failure = query.lastError().text();
            } else {
                query.bindValue(QStringLiteral(":waived"), waived);
                query.bindValue(QStringLiteral(":payment_folio"), paymentFolio);
                if (!query.exec()) {
                    failure = query.lastError().text();
                } else if (query.numRowsAffected() == 0) {
                    failure = QStringLiteral("No existe un pago con folio %1.").arg(paymentFolio);
                } else {
                    updated = true;
                }
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
    } else if (error) {
        error->clear();
    }
    return updated;
}
