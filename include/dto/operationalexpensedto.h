#ifndef OPERATIONALEXPENSEDTO_H
#define OPERATIONALEXPENSEDTO_H

#include <QDate>
#include <QString>

// Gasto operativo. El catálogo operational_expense_cat no tiene DTO (Rule 1);
// el cálculo de IVA/total lo hace el servicio, no este struct.
struct OperationalExpenseDTO
{
    int folio = 0;
    QDate expenseDate;
    int expenseId = -1;
    QString description;
    bool hasInvoice = false;
    QString invoiceNumber;
    double subtotal = 0.0;
    double iva = 0.0;
    double totalAmount = 0.0;
};

#endif // OPERATIONALEXPENSEDTO_H
