#ifndef APPLICATION_FINANCE_PORTS_OPERATIONALEXPENSEREPOSITORY_H
#define APPLICATION_FINANCE_PORTS_OPERATIONALEXPENSEREPOSITORY_H

#include "application/finance/dto/financedtos.h"
#include "domain/finance/model/operationalexpense.h"

#include <QDate>
#include <QList>
#include <QString>

namespace application {

// Puerto: los gastos operativos (operational_expenses) y su catálogo de tipos
// (operational_expense_cat).
class OperationalExpenseRepository
{
public:
    virtual ~OperationalExpenseRepository() = default;

    // El catálogo de tipos, por nombre. Si la consulta falla devuelve vacío y
    // deja la causa en `error`.
    virtual QList<ExpenseTypeOptionDto> expenseTypes(QString *error) = 0;

    // Los gastos del filtro, del más reciente al más viejo. Si la consulta
    // falla devuelve vacío y deja la causa en `error`.
    virtual QList<OperationalExpenseItemDto> list(const OperationalExpenseFilterDto &filter,
                                                  QString *error) = 0;

    // La suma de total_amount de los gastos con fecha en [from, to). Si la
    // consulta falla devuelve 0 y deja la causa en `error`.
    virtual double totalBetween(const QDate &from, const QDate &to, QString *error) = 0;

    // Los años distintos en que hay gastos, del más reciente al más viejo. Si
    // la consulta falla devuelve vacío y deja la causa en `error`.
    virtual QList<int> expenseYears(QString *error) = 0;

    struct AddOutcome
    {
        bool ok = false;
        int folio = -1;
        QString errorMessage;
    };
    // Guarda un gasto ya validado, con la descripción de su tipo.
    virtual AddOutcome add(const domain::OperationalExpense &expense) = 0;

    // Reemplaza la factura del gasto `folio`, ya validada. Sin factura guarda
    // el número como NULL. Devuelve false, con la causa en `error`, si falló
    // la base o el renglón ya no existe.
    virtual bool updateInvoice(int folio, bool hasInvoice, const QString &invoiceNumber,
                               QString *error) = 0;

protected:
    OperationalExpenseRepository() = default;
    OperationalExpenseRepository(const OperationalExpenseRepository &) = default;
    OperationalExpenseRepository &operator=(const OperationalExpenseRepository &) = default;
};

} // namespace application

#endif // APPLICATION_FINANCE_PORTS_OPERATIONALEXPENSEREPOSITORY_H
