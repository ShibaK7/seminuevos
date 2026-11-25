#ifndef APPLICATION_FINANCE_PORTS_EXPENSERECEIPTGENERATOR_H
#define APPLICATION_FINANCE_PORTS_EXPENSERECEIPTGENERATOR_H

#include "application/finance/dto/financedtos.h"

#include <QString>

namespace application {

// Puerto: cómo se produce el recibo de pago de un gasto operativo. Hoy, un
// PDF con QPrinter sobre la plantilla receipt_expense.html, igual que el
// recibo de mantenimiento.
class ExpenseReceiptGenerator
{
public:
    struct Outcome
    {
        bool ok = false;
        QString errorMessage;
    };

    virtual ~ExpenseReceiptGenerator() = default;

    virtual Outcome generate(const OperationalExpenseItemDto &expense,
                             const QString &outputPath) = 0;

protected:
    ExpenseReceiptGenerator() = default;
    ExpenseReceiptGenerator(const ExpenseReceiptGenerator &) = default;
    ExpenseReceiptGenerator &operator=(const ExpenseReceiptGenerator &) = default;
};

} // namespace application

#endif // APPLICATION_FINANCE_PORTS_EXPENSERECEIPTGENERATOR_H
