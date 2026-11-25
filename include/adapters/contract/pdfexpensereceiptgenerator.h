#ifndef ADAPTERS_CONTRACT_PDFEXPENSERECEIPTGENERATOR_H
#define ADAPTERS_CONTRACT_PDFEXPENSERECEIPTGENERATOR_H

#include "application/finance/ports/expensereceiptgenerator.h"

// Adaptador del puerto ExpenseReceiptGenerator: genera el PDF del recibo de
// pago a partir de la plantilla HTML empaquetada como recurso
// (:/templates/receipt_expense.html), sustituyendo sus marcas con los datos
// del renglón.
//
// Marcas obligatorias (si una queda sin llenar, no se genera): [Fecha],
// [Total], [Total_Letra], [Tipo_Gasto] y [Descripción]. Opcionales, por si la
// plantilla las usa: [Folio], [Sub_Total], [IVA], [No_Factura] y
// [Método_Pago]. Los acentos van tal cual en la plantilla, no como entidades.
class PdfExpenseReceiptGenerator final : public application::ExpenseReceiptGenerator
{
public:
    Outcome generate(const application::OperationalExpenseItemDto &expense,
                     const QString &outputPath) override;

    // La plantilla con los datos del gasto, lista para dibujarse. Vacía si no
    // se pudo llenar completa, con la causa en `error`.
    static QString fillTemplate(const application::OperationalExpenseItemDto &expense,
                                QString *error);
};

#endif // ADAPTERS_CONTRACT_PDFEXPENSERECEIPTGENERATOR_H
