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
       s.total_amount,
       COALESCE(fin.down_payment, 0) AS down_payment,
       COALESCE(fin.amount_to_finance, 0) AS amount_to_finance,
       COALESCE(fin.down_payment, 0) + COALESCE(payments.paid_amount, 0) AS paid_amount,
       COALESCE(payments.capital_paid, 0) AS capital_paid,
       COALESCE(payments.interest_paid, 0) AS interest_paid,
       s.total_amount - COALESCE(fin.down_payment, 0)
           - COALESCE(payments.paid_amount, 0) AS balance_due,
       COALESCE(payments.paid_installments, 0) AS paid_installments,
       COALESCE(fin.months_term, 0) AS expected_installments
  FROM sales s
  JOIN vehicles v ON v.folio = s.vehicle_folio
  LEFT JOIN brands_cat brand ON brand.id = v.brand_id
  JOIN counterparties buyer ON buyer.id = s.buyer_id
  LEFT JOIN sale_financing fin ON fin.sale_folio = s.folio
  LEFT JOIN LATERAL (
      SELECT SUM(payment.paid_amount) AS paid_amount,
             SUM(payment.capital_allocation) AS capital_paid,
             SUM(payment.interest_allocation) AS interest_paid,
             COUNT(*) FILTER (WHERE payment.paid_amount > 0) AS paid_installments
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
   AND (paid_amount > 0 OR is_waived)
 ORDER BY installment_number, id
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
