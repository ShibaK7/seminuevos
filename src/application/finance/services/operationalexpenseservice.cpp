#include "application/finance/services/operationalexpenseservice.h"

#include "domain/finance/model/operationalexpense.h"
#include "domain/finance/rules/financeaccess.h"
#include "domain/finance/value_objects/expensetrend.h"

#include <QDate>

#include <algorithm>
#include <functional>

namespace application {

namespace {

bool canUseFinance(domain::UserRole role)
{
    return domain::FinanceAccess(role).canUseFinance();
}

} // namespace

OperationalExpenseService::OperationalExpenseService(OperationalExpenseRepository &expenses,
                                                     ExpenseReceiptGenerator &receipts)
    : m_expenses(expenses)
    , m_receipts(receipts)
{
}

ExpenseFormLookupsResult OperationalExpenseService::formLookups(domain::UserRole role) const
{
    using Status = ExpenseFormLookupsResult::Status;
    ExpenseFormLookupsResult result;
    if (!canUseFinance(role)) {
        result.status = Status::Denied;
        return result;
    }
    QString error;
    result.expenseTypes = m_expenses.expenseTypes(&error);
    if (!error.isEmpty()) {
        result.status = Status::Failed;
        result.expenseTypes.clear();
        result.errorMessage = error;
        return result;
    }
    result.status = Status::Loaded;
    return result;
}

OperationalExpenseListResult OperationalExpenseService::list(
    const OperationalExpenseFilterDto &filter, domain::UserRole role) const
{
    using Status = OperationalExpenseListResult::Status;
    OperationalExpenseListResult result;
    if (!canUseFinance(role)) {
        result.status = Status::Denied;
        return result;
    }
    // El mes que evalúa la tarjeta: el del filtro si es un mes concreto; con
    // "Todo" en cualquiera de los dos combos, el mes en curso.
    const QDate today = QDate::currentDate();
    const QDate evaluated = filter.month && filter.year
                                ? QDate(*filter.year, *filter.month, 1)
                                : QDate(today.year(), today.month(), 1);

    QString error;
    result.items = m_expenses.list(filter, &error);
    if (error.isEmpty())
        result.years = m_expenses.expenseYears(&error);
    if (error.isEmpty())
        result.summary.total = m_expenses.totalBetween(evaluated, evaluated.addMonths(1), &error);
    if (error.isEmpty())
        result.summary.previousTotal =
            m_expenses.totalBetween(evaluated.addMonths(-1), evaluated, &error);
    if (!error.isEmpty()) {
        result.status = Status::Failed;
        result.items.clear();
        result.years.clear();
        result.errorMessage = error;
        return result;
    }
    // El año en curso siempre está: en una base nueva el filtro no queda
    // vacío, y es el año en que se registra casi todo.
    result.summary.year = evaluated.year();
    result.summary.month = evaluated.month();
    result.summary.changePercent = domain::ExpenseTrend::percentChange(
        result.summary.total, result.summary.previousTotal);

    const int currentYear = today.year();
    if (!result.years.contains(currentYear)) {
        result.years << currentYear;
        std::sort(result.years.begin(), result.years.end(), std::greater<int>());
    }
    result.status = Status::Loaded;
    return result;
}

OperationalExpenseAddResult OperationalExpenseService::add(
    const NewOperationalExpenseDto &expense, domain::UserRole role) const
{
    using Status = OperationalExpenseAddResult::Status;
    OperationalExpenseAddResult result;
    if (!canUseFinance(role)) {
        result.status = Status::Denied;
        return result;
    }

    const domain::OperationalExpense entity(expense.date, expense.expenseTypeId.value_or(-1),
                                            expense.hasInvoice, expense.invoiceNumber,
                                            expense.subtotal, expense.paymentMethod);
    const domain::ValidationResult validation = entity.validate();
    if (!validation.isValid()) {
        result.status = Status::Rejected;
        result.validation = validation;
        return result;
    }

    const OperationalExpenseRepository::AddOutcome outcome = m_expenses.add(entity);
    if (!outcome.ok) {
        result.status = Status::Failed;
        result.errorMessage = outcome.errorMessage;
        return result;
    }
    result.status = Status::Saved;
    result.folio = outcome.folio;
    return result;
}

OperationalExpenseInvoiceUpdateResult OperationalExpenseService::updateInvoice(
    int folio, bool hasInvoice, const QString &invoiceNumber, domain::UserRole role) const
{
    using Status = OperationalExpenseInvoiceUpdateResult::Status;
    OperationalExpenseInvoiceUpdateResult result;
    if (!canUseFinance(role)) {
        result.status = Status::Denied;
        return result;
    }

    const domain::ValidationResult validation =
        domain::OperationalExpense::validateInvoice(hasInvoice, invoiceNumber);
    if (!validation.isValid()) {
        result.status = Status::Rejected;
        result.validation = validation;
        return result;
    }

    QString error;
    if (!m_expenses.updateInvoice(folio, hasInvoice,
                                  hasInvoice ? invoiceNumber.trimmed() : QString(), &error)) {
        result.status = Status::Failed;
        result.errorMessage = error;
        return result;
    }
    result.status = Status::Updated;
    return result;
}

ExpenseReceiptGenerator::Outcome OperationalExpenseService::generateReceipt(
    const OperationalExpenseItemDto &expense, const QString &outputPath,
    domain::UserRole role) const
{
    if (!canUseFinance(role)) {
        ExpenseReceiptGenerator::Outcome denied;
        denied.errorMessage = QStringLiteral("Solo el Administrador puede usar Finanzas.");
        return denied;
    }
    return m_receipts.generate(expense, outputPath);
}

} // namespace application
