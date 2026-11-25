#ifndef ADAPTERS_PERSISTENCE_FINANCE_SQLOPERATIONALEXPENSEREPOSITORY_H
#define ADAPTERS_PERSISTENCE_FINANCE_SQLOPERATIONALEXPENSEREPOSITORY_H

#include "application/finance/ports/operationalexpenserepository.h"

class ConnectionPool;

// Adaptador del puerto OperationalExpenseRepository sobre operational_expenses
// y operational_expense_cat.
//
// La descripción del gasto se copia del tipo dentro del mismo INSERT: así lo
// guardado es lo que decía el catálogo al darlo de alta, aunque el catálogo
// cambie después, y un tipo que ya no existe hace fallar el INSERT por su
// llave foránea en vez de guardar un gasto sin tipo.
class SqlOperationalExpenseRepository final : public application::OperationalExpenseRepository
{
public:
    explicit SqlOperationalExpenseRepository(ConnectionPool &pool);

    QList<application::ExpenseTypeOptionDto> expenseTypes(QString *error) override;
    QList<application::OperationalExpenseItemDto> list(
        const application::OperationalExpenseFilterDto &filter, QString *error) override;
    double totalBetween(const QDate &from, const QDate &to, QString *error) override;
    QList<int> expenseYears(QString *error) override;
    AddOutcome add(const domain::OperationalExpense &expense) override;
    bool updateInvoice(int folio, bool hasInvoice, const QString &invoiceNumber,
                       QString *error) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_FINANCE_SQLOPERATIONALEXPENSEREPOSITORY_H
