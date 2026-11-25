#include "adapters/persistence/finance/sqloperationalexpenserepository.h"

#include "adapters/persistence/connectionpool.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

QVariant nullText()
{
    return QVariant(QMetaType::fromType<QString>());
}

} // namespace

SqlOperationalExpenseRepository::SqlOperationalExpenseRepository(ConnectionPool &pool)
    : m_pool(pool)
{
}

QList<application::ExpenseTypeOptionDto> SqlOperationalExpenseRepository::expenseTypes(
    QString *error)
{
    QList<application::ExpenseTypeOptionDto> options;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        if (!db.isOpen()
            || !query.exec(QStringLiteral(
                "SELECT id, name, description FROM operational_expense_cat ORDER BY name"))) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                options << application::ExpenseTypeOptionDto{query.value(0).toInt(),
                                                             query.value(1).toString(),
                                                             query.value(2).toString()};
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        options.clear();
    }
    return options;
}

QList<application::OperationalExpenseItemDto> SqlOperationalExpenseRepository::list(
    const application::OperationalExpenseFilterDto &filter, QString *error)
{
    // Con año, el filtro es un rango de fechas y no EXTRACT(): así usa el
    // índice de expense_date. Solo un mes sin año (ese mes de todos los años)
    // necesita EXTRACT.
    QStringList conditions;
    QDate from;
    QDate to;
    if (filter.year) {
        from = QDate(*filter.year, filter.month.value_or(1), 1);
        to = filter.month ? from.addMonths(1) : from.addYears(1);
        conditions << QStringLiteral("e.expense_date >= :from AND e.expense_date < :to");
    } else if (filter.month) {
        conditions << QStringLiteral("EXTRACT(MONTH FROM e.expense_date) = :month");
    }

    QList<application::OperationalExpenseItemDto> items;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT e.folio, e.expense_date, c.name, e.description, e.has_invoice, "
            "e.invoice_number, e.subtotal, e.iva, e.total_amount, e.payment_method "
            "FROM operational_expenses e "
            "JOIN operational_expense_cat c ON c.id = e.expense_id %1"
            "ORDER BY e.expense_date DESC, e.folio DESC")
                          .arg(conditions.isEmpty()
                                   ? QString()
                                   : QStringLiteral("WHERE %1 ").arg(conditions.join(
                                         QStringLiteral(" AND ")))));
        if (filter.year) {
            query.bindValue(QStringLiteral(":from"), from);
            query.bindValue(QStringLiteral(":to"), to);
        } else if (filter.month) {
            query.bindValue(QStringLiteral(":month"), *filter.month);
        }
        if (!db.isOpen() || !query.exec()) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next()) {
                application::OperationalExpenseItemDto item;
                item.folio = query.value(0).toInt();
                item.date = query.value(1).toDate();
                item.typeName = query.value(2).toString();
                item.description = query.value(3).toString();
                item.hasInvoice = query.value(4).toBool();
                item.invoiceNumber = query.value(5).toString();
                item.subtotal = query.value(6).toDouble();
                item.iva = query.value(7).toDouble();
                item.total = query.value(8).toDouble();
                item.paymentMethod = domain::paymentMethodFromDb(query.value(9).toString());
                items << item;
            }
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        items.clear();
    }
    return items;
}

double SqlOperationalExpenseRepository::totalBetween(const QDate &from, const QDate &to,
                                                     QString *error)
{
    double total = 0.0;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        // COALESCE: SUM de ningún renglón es NULL, no 0.
        query.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(total_amount), 0) FROM operational_expenses "
            "WHERE expense_date >= :from AND expense_date < :to"));
        query.bindValue(QStringLiteral(":from"), from);
        query.bindValue(QStringLiteral(":to"), to);
        if (!db.isOpen() || !query.exec() || !query.next())
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        else
            total = query.value(0).toDouble();
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure.isEmpty() ? QStringLiteral("Error desconocido") : failure;
        total = 0.0;
    }
    return total;
}

QList<int> SqlOperationalExpenseRepository::expenseYears(QString *error)
{
    QList<int> years;
    QString failure;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        // La base es la única que sabe qué años existen: así un gasto
        // agregado, corregido o borrado se refleja en la siguiente lectura.
        if (!db.isOpen()
            || !query.exec(QStringLiteral(
                "SELECT DISTINCT CAST(EXTRACT(YEAR FROM expense_date) AS INTEGER) "
                "FROM operational_expenses ORDER BY 1 DESC"))) {
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else {
            while (query.next())
                years << query.value(0).toInt();
        }
    }
    if (!failure.isEmpty()) {
        m_pool.discardThreadConnection();
        if (error)
            *error = failure;
        years.clear();
    }
    return years;
}

application::OperationalExpenseRepository::AddOutcome SqlOperationalExpenseRepository::add(
    const domain::OperationalExpense &expense)
{
    AddOutcome result;
    bool databaseFailed = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        if (!db.isOpen()) {
            databaseFailed = true;
            result.errorMessage = QStringLiteral("No se pudo conectar con la base de datos: %1")
                                      .arg(db.lastError().text());
        } else {
            // Ya validado: los importes y el método de pago siempre vienen.
            const domain::ExpenseAmounts &amounts = *expense.amounts();
            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT INTO operational_expenses "
                "(expense_date, expense_id, description, has_invoice, invoice_number, subtotal, "
                "iva, total_amount, payment_method) "
                "VALUES (:date, :type, "
                "(SELECT description FROM operational_expense_cat WHERE id = :typeForDescription), "
                ":hasInvoice, :invoiceNumber, CAST(:subtotal AS NUMERIC(12, 2)), "
                "CAST(:iva AS NUMERIC(12, 2)), CAST(:total AS NUMERIC(12, 2)), :paymentMethod) "
                "RETURNING folio"));
            insert.bindValue(QStringLiteral(":date"), expense.date());
            insert.bindValue(QStringLiteral(":type"), expense.typeId());
            insert.bindValue(QStringLiteral(":typeForDescription"), expense.typeId());
            insert.bindValue(QStringLiteral(":hasInvoice"), expense.hasInvoice());
            // Sin factura se guarda NULL, no una cadena vacía.
            insert.bindValue(QStringLiteral(":invoiceNumber"),
                             expense.hasInvoice() ? QVariant(expense.invoiceNumber()) : nullText());
            insert.bindValue(QStringLiteral(":subtotal"), amounts.subtotal());
            insert.bindValue(QStringLiteral(":iva"), amounts.iva());
            insert.bindValue(QStringLiteral(":total"), amounts.total());
            insert.bindValue(QStringLiteral(":paymentMethod"),
                             domain::toDbString(*expense.paymentMethod()));
            if (insert.exec() && insert.next()) {
                result.ok = true;
                result.folio = insert.value(0).toInt();
            } else {
                databaseFailed = true;
                result.errorMessage = QStringLiteral("No se pudo guardar el gasto: %1")
                                          .arg(insert.lastError().text());
            }
        }
    }
    // Con la conexión ya soltada, una que falló se descarta para que el
    // reintento abra otra.
    if (databaseFailed)
        m_pool.discardThreadConnection();
    return result;
}

bool SqlOperationalExpenseRepository::updateInvoice(int folio, bool hasInvoice,
                                                    const QString &invoiceNumber, QString *error)
{
    QString failure;
    bool databaseFailed = false;
    {
        ConnectionPool::Handle handle = m_pool.acquire();
        QSqlDatabase &db = handle.database();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("UPDATE operational_expenses "
                                     "SET has_invoice = :hasInvoice, invoice_number = :number "
                                     "WHERE folio = :folio"));
        query.bindValue(QStringLiteral(":hasInvoice"), hasInvoice);
        query.bindValue(QStringLiteral(":number"),
                        hasInvoice ? QVariant(invoiceNumber) : nullText());
        query.bindValue(QStringLiteral(":folio"), folio);
        if (!db.isOpen() || !query.exec()) {
            databaseFailed = true;
            failure = db.isOpen() ? query.lastError().text() : db.lastError().text();
        } else if (query.numRowsAffected() != 1) {
            failure = QStringLiteral("El gasto ya no existe.");
        }
    }
    // Solo una falla de la base descarta la conexión: un renglón que ya no
    // existe no dice nada de ella.
    if (databaseFailed)
        m_pool.discardThreadConnection();
    if (!failure.isEmpty() && error)
        *error = failure;
    return failure.isEmpty();
}
