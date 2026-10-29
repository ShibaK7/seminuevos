#include "adapters/persistence/commercial/sqlsalerepository.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

application::SaleVehicleOptionDto readVehicleOption(const QSqlQuery &query)
{
    application::SaleVehicleOptionDto result;
    result.folio = query.value(0).toInt();
    result.brandName = query.value(1).toString();
    result.model = query.value(2).toString();
    result.yearModel = query.value(3).toInt();
    const QVariant price = query.value(4);
    if (!price.isNull())
        result.salePrice = price.toDouble();

    result.serialNumber = query.value(5).toString();
    result.motorNumber = query.value(6).toString();
    result.plates = query.value(7).toString();
    result.color = query.value(8).toString();
    return result;
}

application::SaleVendorOptionDto readVendorOption(const QSqlQuery &query)
{
    application::SaleVendorOptionDto result;
    result.id = query.value(0).toInt();
    result.displayName = query.value(1).toString();
    result.username = query.value(2).toString();
    return result;
}

QVariant nullIfEmpty(const QString &value)
{
    if (value.isEmpty())
        return QVariant(QMetaType(QMetaType::QString));
    return value;
}

int findExistingCounterpartyId(QSqlDatabase &db, const QString &nationalId, QString *error)
{
    const QString trimmed = nationalId.trimmed();
    if (trimmed.isEmpty())
        return -1;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id FROM counterparties "
        "WHERE UPPER(BTRIM(COALESCE(national_id, ''))) = :national_id LIMIT 1"));
    query.bindValue(QStringLiteral(":national_id"), trimmed.toUpper());
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return -1;
    }
    if (!query.next())
        return -1;
    return query.value(0).toInt();
}

int insertCounterparty(QSqlDatabase &db, const application::SaleBuyerDto &person, QString *error)
{
    if (person.counterpartyId > 0)
        return person.counterpartyId;

    const int existingId = findExistingCounterpartyId(db, person.nationalId, error);
    if (existingId > 0) {
        if (error)
            *error = QStringLiteral("Ya existe una persona registrada con ese número de identificación.");
        return -1;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO counterparties (full_name, national_id, street_address, suburb, locality, "
        "state, postal_code, phone, email) VALUES (:full_name, :national_id, :street_address, "
        ":suburb, :locality, :state, :postal_code, :phone, :email) RETURNING id"));
    query.bindValue(QStringLiteral(":full_name"), person.fullName.trimmed());
    query.bindValue(QStringLiteral(":national_id"), nullIfEmpty(person.nationalId.trimmed()));
    query.bindValue(QStringLiteral(":street_address"), nullIfEmpty(person.streetAddress.trimmed()));
    query.bindValue(QStringLiteral(":suburb"), nullIfEmpty(person.suburb.trimmed()));
    query.bindValue(QStringLiteral(":locality"), nullIfEmpty(person.locality.trimmed()));
    query.bindValue(QStringLiteral(":state"), nullIfEmpty(person.state.trimmed()));
    query.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(person.postalCode.trimmed()));
    query.bindValue(QStringLiteral(":phone"), nullIfEmpty(person.phone.trimmed()));
    query.bindValue(QStringLiteral(":email"), nullIfEmpty(person.email.trimmed()));
    if (!query.exec() || !query.next()) {
        if (error)
            *error = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}

bool updateCounterparty(QSqlDatabase &db, int id, const application::SaleBuyerDto &person,
                       QString *error)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE counterparties SET full_name = :full_name, national_id = :national_id, "
        "street_address = :street_address, suburb = :suburb, locality = :locality, "
        "state = :state, postal_code = :postal_code, phone = :phone, email = :email, "
        "updated_at = CURRENT_TIMESTAMP WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":full_name"), person.fullName.trimmed());
    query.bindValue(QStringLiteral(":national_id"), nullIfEmpty(person.nationalId.trimmed()));
    query.bindValue(QStringLiteral(":street_address"), nullIfEmpty(person.streetAddress.trimmed()));
    query.bindValue(QStringLiteral(":suburb"), nullIfEmpty(person.suburb.trimmed()));
    query.bindValue(QStringLiteral(":locality"), nullIfEmpty(person.locality.trimmed()));
    query.bindValue(QStringLiteral(":state"), nullIfEmpty(person.state.trimmed()));
    query.bindValue(QStringLiteral(":postal_code"), nullIfEmpty(person.postalCode.trimmed()));
    query.bindValue(QStringLiteral(":phone"), nullIfEmpty(person.phone.trimmed()));
    query.bindValue(QStringLiteral(":email"), nullIfEmpty(person.email.trimmed()));
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        if (error)
            *error = QStringLiteral("No se encontró el registro de la persona seleccionada.");
        return false;
    }
    return true;
}

} // namespace

SqlSaleRepository::SqlSaleRepository(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::SaleVehicleOptionDto> SqlSaleRepository::searchAvailableVehicles(
    const QString &text, QString *error)
{
    QList<application::SaleVehicleOptionDto> results;
    QString failure;
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty() && trimmed.size() < 3) {
        if (error)
            error->clear();
        return results;
    }

    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery query(db);
            QString sql = QStringLiteral(
                "SELECT v.folio, b.name AS brand_name, v.model, v.year_model, "
                "COALESCE(acq.sale_price, csg.sale_price) AS sale_price, "
                "v.serial_number, v.motor_number, v.plates, v.color "
                "FROM vehicles v "
                "LEFT JOIN brands_cat b ON b.id = v.brand_id "
                "LEFT JOIN vehicle_acquisitions acq ON acq.vehicle_folio = v.folio "
                "LEFT JOIN vehicle_consignments csg ON csg.vehicle_folio = v.folio "
                "WHERE v.status = 'Disponible'");
            if (!trimmed.isEmpty()) {
                sql += QStringLiteral(
                    " AND (LOWER(COALESCE(b.name, '')) LIKE :term "
                    "OR LOWER(v.model) LIKE :term "
                    "OR LOWER(COALESCE(v.serial_number, '')) LIKE :term "
                    "OR LOWER(COALESCE(v.plates, '')) LIKE :term)");
            }
            sql += QStringLiteral(
                " ORDER BY v.added_date DESC NULLS LAST, v.folio DESC LIMIT :limit");

            if (!query.prepare(sql)) {
                failure = query.lastError().text();
            } else {
                if (!trimmed.isEmpty()) {
                    query.bindValue(QStringLiteral(":term"),
                                    QStringLiteral("%") + trimmed.toLower() + QStringLiteral("%"));
                }
                query.bindValue(QStringLiteral(":limit"), 10);
                if (!query.exec()) {
                    failure = query.lastError().text();
                } else {
                    while (query.next())
                        results << readVehicleOption(query);
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
    return results;
}

QList<application::SaleVendorOptionDto> SqlSaleRepository::searchVendors(
    const QString &text, QString *error)
{
    QList<application::SaleVendorOptionDto> results;
    QString failure;
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty() && trimmed.size() < 3) {
        if (error)
            error->clear();
        return results;
    }

    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else {
            QSqlQuery query(db);
            QString sql = QStringLiteral(
                "SELECT id, display_name, username FROM users WHERE 1 = 1");
            if (!trimmed.isEmpty()) {
                sql += QStringLiteral(
                    " AND (LOWER(display_name) LIKE :term OR LOWER(username) LIKE :term)");
            }
            sql += QStringLiteral(" ORDER BY last_login DESC NULLS LAST, display_name LIMIT :limit");

            if (!query.prepare(sql)) {
                failure = query.lastError().text();
            } else {
                if (!trimmed.isEmpty())
                    query.bindValue(QStringLiteral(":term"),
                                    QStringLiteral("%") + trimmed.toLower() + QStringLiteral("%"));
                query.bindValue(QStringLiteral(":limit"), 10);
                if (!query.exec()) {
                    failure = query.lastError().text();
                } else {
                    while (query.next())
                        results << readVendorOption(query);
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
    return results;
}

application::SaleRegistrationResultDto SqlSaleRepository::registerSale(
    const application::SaleRegistrationDto &sale)
{
    application::SaleRegistrationResultDto result;
    QString failure;

    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            failure = db.lastError().text();
        } else if (!db.transaction()) {
            failure = db.lastError().text();
        } else {
            int buyerId = -1;
            int guarantorId = -1;
            int saleFolio = -1;

            {
                QSqlQuery updateVehicle(db);
                updateVehicle.prepare(QStringLiteral(
                    "UPDATE vehicles SET status = 'Vendido' "
                    "WHERE folio = :folio AND status = 'Disponible'"));
                updateVehicle.bindValue(QStringLiteral(":folio"), sale.vehicleFolio);
                if (!updateVehicle.exec())
                    failure = updateVehicle.lastError().text();
                else if (updateVehicle.numRowsAffected() != 1)
                    failure = QStringLiteral("El vehículo ya no está disponible para la venta.");
            }
            if (failure.isEmpty())
                buyerId = insertCounterparty(db, sale.buyer, &failure);
            if (failure.isEmpty() && sale.updateBuyerRecord)
                updateCounterparty(db, buyerId, sale.buyer, &failure);
            if (failure.isEmpty() && sale.paymentType == domain::PaymentType::Credito
                && sale.financing && sale.financing->guarantor) {
                guarantorId = insertCounterparty(db, *sale.financing->guarantor, &failure);
                if (failure.isEmpty() && sale.updateGuarantorRecord)
                    updateCounterparty(db, guarantorId, *sale.financing->guarantor, &failure);
            }

            if (failure.isEmpty()) {
                QSqlQuery query(db);
                query.prepare(QStringLiteral(
                    "INSERT INTO sales (vehicle_folio, vendor_id, buyer_id, payment_type, "
                    "payment_method, subtotal, iva, total_amount, observations, status, sale_date) "
                    "VALUES (:vehicle_folio, :vendor_id, :buyer_id, :payment_type, "
                    ":payment_method, :subtotal, :iva, :total_amount, :observations, :status, :sale_date) "
                    "RETURNING folio"));
                query.bindValue(QStringLiteral(":vehicle_folio"), sale.vehicleFolio);
                query.bindValue(QStringLiteral(":vendor_id"), sale.vendorId);
                query.bindValue(QStringLiteral(":buyer_id"), buyerId);
                query.bindValue(QStringLiteral(":payment_type"), domain::toDbString(sale.paymentType));
                const QString paymentMethod = sale.paymentType == domain::PaymentType::Credito
                    ? QStringLiteral("Financiamiento")
                    : (sale.paymentMethod ? domain::toDbString(*sale.paymentMethod) : QString());
                query.bindValue(QStringLiteral(":payment_method"), nullIfEmpty(paymentMethod));
                query.bindValue(QStringLiteral(":subtotal"), sale.subtotal);
                query.bindValue(QStringLiteral(":iva"), sale.iva);
                query.bindValue(QStringLiteral(":total_amount"), sale.totalAmount);
                query.bindValue(QStringLiteral(":observations"), nullIfEmpty(sale.observations));
                query.bindValue(QStringLiteral(":status"), sale.paymentType == domain::PaymentType::Contado
                    ? QStringLiteral("Pago Completo") : QStringLiteral("Por Cobrar"));
                query.bindValue(QStringLiteral(":sale_date"), sale.saleDate);

                if (!query.exec() || !query.next())
                    failure = query.lastError().text();
                else
                    saleFolio = query.value(0).toInt();
            }

            if (failure.isEmpty() && sale.paymentType == domain::PaymentType::Credito && sale.financing) {
                QSqlQuery financing(db);
                financing.prepare(QStringLiteral(
                    "INSERT INTO sale_financing (sale_folio, down_payment, amount_to_finance, "
                    "interest_rate, months_term, monthly_payment, final_calculated_price, "
                    "aval_id, status) VALUES (:sale_folio, :down_payment, :amount_to_finance, "
                    ":interest_rate, :months_term, :monthly_payment, :final_calculated_price, "
                    ":aval_id, :status)"));
                const auto &financingData = *sale.financing;
                financing.bindValue(QStringLiteral(":sale_folio"), saleFolio);
                financing.bindValue(QStringLiteral(":down_payment"), financingData.downPayment);
                financing.bindValue(QStringLiteral(":amount_to_finance"), financingData.amountToFinance);
                financing.bindValue(QStringLiteral(":interest_rate"), financingData.interestRatePercent);
                financing.bindValue(QStringLiteral(":months_term"), financingData.monthsTerm);
                financing.bindValue(QStringLiteral(":monthly_payment"), financingData.monthlyPayment);
                financing.bindValue(QStringLiteral(":final_calculated_price"), financingData.finalPrice);
                financing.bindValue(QStringLiteral(":aval_id"), guarantorId > 0
                    ? QVariant(guarantorId) : QVariant(QMetaType(QMetaType::Int)));
                financing.bindValue(QStringLiteral(":status"), QStringLiteral("Al Corriente"));
                if (!financing.exec())
                    failure = financing.lastError().text();

                for (const application::SaleInstallmentDto &installment : financingData.installments) {
                    if (!failure.isEmpty())
                        break;
                    QSqlQuery payment(db);
                    payment.prepare(QStringLiteral(
                        "INSERT INTO sale_payments (sale_folio, installment_number, scheduled_date, "
                        "expected_amount, capital_allocation, interest_allocation, remaining_balance) "
                        "VALUES (:sale_folio, :installment_number, :scheduled_date, :expected_amount, "
                        ":capital_allocation, :interest_allocation, :remaining_balance)"));
                    payment.bindValue(QStringLiteral(":sale_folio"), saleFolio);
                    payment.bindValue(QStringLiteral(":installment_number"), installment.number);
                    payment.bindValue(QStringLiteral(":scheduled_date"), installment.dueDate);
                    payment.bindValue(QStringLiteral(":expected_amount"), installment.expectedAmount);
                    payment.bindValue(QStringLiteral(":capital_allocation"), installment.capitalAllocation);
                    payment.bindValue(QStringLiteral(":interest_allocation"), installment.interestAllocation);
                    payment.bindValue(QStringLiteral(":remaining_balance"), installment.remainingBalance);
                    if (!payment.exec())
                        failure = payment.lastError().text();
                }
            }

            if (failure.isEmpty() && !db.commit())
                failure = db.lastError().text();
            if (!failure.isEmpty()) {
                db.rollback();
            } else {
                result.saleFolio = saleFolio;
            }
        }
    }

    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        result.status = application::SaleRegistrationResultDto::Status::Failed;
        result.message = failure;
        return result;
    }

    result.status = application::SaleRegistrationResultDto::Status::Registered;
    return result;
}
