#ifndef APPLICATION_FINANCE_SERVICES_OPERATIONALEXPENSESERVICE_H
#define APPLICATION_FINANCE_SERVICES_OPERATIONALEXPENSESERVICE_H

#include "application/finance/dto/financedtos.h"
#include "application/finance/ports/expensereceiptgenerator.h"
#include "application/finance/ports/operationalexpenserepository.h"
#include "domain/common/value_objects/enums.h"

namespace application {

// Caso de uso "llevar los gastos operativos" (Finanzas, Gastos Generales):
// leer la tabla, dar de alta un gasto y corregir su factura. Cada operación
// exige primero que el rol pueda usar Finanzas (FinanceAccess): ocultar el
// menú no basta, y el presenter no puede preguntarle al dominio.
//
// Solo guarda referencias a sus puertos, así que se puede llamar desde el
// hilo de trabajo. Todas sus operaciones hacen E/S; salvo el recibo, correrlas
// fuera del hilo de la interfaz.
class OperationalExpenseService final
{
public:
    OperationalExpenseService(OperationalExpenseRepository &expenses,
                              ExpenseReceiptGenerator &receipts);

    // El catálogo del formulario de alta.
    ExpenseFormLookupsResult formLookups(domain::UserRole role) const;

    // Los gastos del filtro, los años que llenan el combo (de toda la tabla,
    // no del filtro) y el resumen del mes evaluado: el del filtro si trae mes
    // y año, si no el mes en curso.
    OperationalExpenseListResult list(const OperationalExpenseFilterDto &filter,
                                      domain::UserRole role) const;

    // Da de alta un gasto: el rol, las reglas del dominio (sin tocar la base) y
    // después el renglón, con el IVA y el total que calcula el dominio.
    OperationalExpenseAddResult add(const NewOperationalExpenseDto &expense,
                                    domain::UserRole role) const;

    // Corrige la factura de un gasto ya guardado: el rol, la regla del dominio y
    // después la base. Sin factura, el número se borra.
    OperationalExpenseInvoiceUpdateResult updateInvoice(int folio, bool hasInvoice,
                                                        const QString &invoiceNumber,
                                                        domain::UserRole role) const;

    // El recibo de pago de un gasto de la tabla, en outputPath. Como el
    // recibo de mantenimiento, corre en el hilo de la interfaz: QTextDocument
    // y QPrinter dibujan ahí, y no toca la base.
    ExpenseReceiptGenerator::Outcome generateReceipt(const OperationalExpenseItemDto &expense,
                                                     const QString &outputPath,
                                                     domain::UserRole role) const;

private:
    OperationalExpenseRepository &m_expenses;
    ExpenseReceiptGenerator &m_receipts;
};

} // namespace application

#endif // APPLICATION_FINANCE_SERVICES_OPERATIONALEXPENSESERVICE_H
