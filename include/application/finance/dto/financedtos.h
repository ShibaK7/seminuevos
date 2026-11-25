#ifndef APPLICATION_FINANCE_DTO_FINANCEDTOS_H
#define APPLICATION_FINANCE_DTO_FINANCEDTOS_H

#include "domain/common/value_objects/enums.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace application {

// DTOs de los gastos operativos de Finanzas ("Gastos Generales"). Structs
// planos, sin lógica.

// Una opción de operational_expense_cat. A diferencia de CatalogOptionDto trae
// la descripción: el formulario de alta la muestra al elegir el tipo, y es la
// que se guarda con el gasto.
struct ExpenseTypeOptionDto
{
    int id = -1;
    QString name;
    QString description;
};

// Lo que "Agregar Gasto" necesita leer antes de abrirse.
struct ExpenseFormLookupsResult
{
    enum class Status {
        Loaded, // expenseTypes trae el catálogo
        Denied, // el rol no usa Finanzas
        Failed, // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    QList<ExpenseTypeOptionDto> expenseTypes;
    QString errorMessage; // en Failed
};

// Los filtros de la tabla. nullopt es "Todo". Un mes sin año trae ese mes de
// todos los años.
struct OperationalExpenseFilterDto
{
    std::optional<int> month; // 1 a 12
    std::optional<int> year;
};

// La tarjeta "Total Egresos": cuánto se gastó en el mes evaluado y cuánto
// cambió contra el mes anterior.
struct MonthlyExpenseSummaryDto
{
    int year = 0;
    int month = 0; // 1 a 12
    double total = 0.0;
    double previousTotal = 0.0;
    // Porcentaje contra el mes anterior; nullopt si el mes anterior no tuvo
    // gastos y este sí (no hay base para comparar).
    std::optional<double> changePercent;
};

// Un renglón de la tabla de gastos, listo para pintarse.
struct OperationalExpenseItemDto
{
    int folio = -1;
    QDate date;
    QString typeName;
    QString description;
    bool hasInvoice = false;
    QString invoiceNumber;
    double subtotal = 0.0;
    double iva = 0.0;
    double total = 0.0;
    // nullopt si la base trae un texto que el dominio no conoce.
    std::optional<domain::PaymentMethod> paymentMethod;
};

struct OperationalExpenseListResult
{
    enum class Status {
        Loaded, // items trae los gastos del filtro, del más reciente al más viejo
        Denied, // el rol no usa Finanzas
        Failed, // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    QList<OperationalExpenseItemDto> items;
    // Los años que tienen gastos registrados, del más reciente al más viejo,
    // más el año en curso aunque todavía no tenga ninguno. Llenan el filtro
    // de año: como llegan en cada lectura, un gasto de un año nuevo aparece
    // en el combo con la recarga que sigue al alta, sin comparar nada.
    QList<int> years;
    MonthlyExpenseSummaryDto summary;
    QString errorMessage; // en Failed
};

// Lo capturado en "Agregar Gasto". El subtotal va como el texto que se
// escribió: el dominio decide si es un importe válido y calcula IVA y total.
// La descripción no viaja: es la del tipo de gasto elegido.
struct NewOperationalExpenseDto
{
    QDate date;
    std::optional<int> expenseTypeId; // sin elegir
    bool hasInvoice = false;
    QString invoiceNumber;
    QString subtotal;
    std::optional<domain::PaymentMethod> paymentMethod; // sin elegir
};

struct OperationalExpenseAddResult
{
    enum class Status {
        Saved,    // folio es el que asignó la base
        Rejected, // validation dice qué campos corregir; no se tocó la base
        Denied,   // el rol no usa Finanzas
        Failed,   // falló la base y no se guardó nada; se puede reintentar
    };
    Status status = Status::Failed;
    int folio = -1;
    domain::ValidationResult validation; // en Rejected
    QString errorMessage;                 // en Failed
};

// Corregir la factura (casilla y número) de un gasto ya guardado.
struct OperationalExpenseInvoiceUpdateResult
{
    enum class Status {
        Updated,  // quedó guardada
        Rejected, // validation dice por qué; no se tocó la base
        Denied,   // el rol no usa Finanzas
        Failed,   // falló la base o el renglón ya no existe; no se guardó
    };
    Status status = Status::Failed;
    domain::ValidationResult validation; // en Rejected
    QString errorMessage;                 // en Failed
};

} // namespace application

#endif // APPLICATION_FINANCE_DTO_FINANCEDTOS_H
