#ifndef PRESENTATION_FINANCE_IFINANCEVIEW_H
#define PRESENTATION_FINANCE_IFINANCEVIEW_H

#include "application/finance/dto/financedtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>

namespace presentation {

// La página de Finanzas (Gastos Generales), vista desde su presenter. La
// implementa FinanceView.
class IFinanceView
{
public:
    virtual ~IFinanceView() = default;

    // Mientras se lee lo que necesita el formulario de alta.
    virtual void setBusy(bool busy) = 0;
    // Pinta los renglones de la tabla de gastos. La casilla Factura de cada
    // uno se puede cambiar, y su No. Factura solo se edita si está marcada;
    // si no lo está, la celda dice "Sin Factura".
    virtual void showExpenses(const QList<application::OperationalExpenseItemDto> &items) = 0;
    virtual void showWarning(const QString &title, const QString &message) = 0;
    // Llena el filtro de año: "Todo" y después los años, del más reciente al
    // más viejo. Conserva el elegido si sigue en la lista; si no, vuelve a
    // "Todo" y avisa el filtro nuevo.
    virtual void showYears(const QList<int> &years) = 0;
    // La tarjeta "Total Egresos": el nombre del mes evaluado, su total y el
    // cambio contra el mes anterior.
    virtual void showMonthlySummary(const application::MonthlyExpenseSummaryDto &summary) = 0;
    // Abre para editar la celda No. Factura del gasto `folio`: se acaba de
    // marcar su Factura y falta el número.
    virtual void editInvoiceNumber(int folio) = 0;

    // --- Confirmaciones de la tabla, con Cancelar y Aceptar. true si aceptó ---
    // "¿Está seguro de realizar la actualización correspondiente a la
    // Factura?"
    virtual bool confirmInvoiceUpdate() = 0;
    // "¿Desea realizar la actualización del valor de No. Factura?"
    virtual bool confirmInvoiceNumberUpdate() = 0;
    // Se marcó Factura y se salió de su No. Factura sin capturarlo: alerta que
    // falta el número. true si el usuario va a capturarlo; false si prefiere
    // quitar la factura.
    virtual bool askForMissingInvoiceNumber() = 0;

    // --- El recibo de pago ---
    // El folio del renglón seleccionado en la tabla, o -1 si no hay.
    virtual int selectedExpenseFolio() = 0;
    // Pide dónde guardar el PDF. Vacío si el usuario canceló.
    virtual QString askReceiptPath(const QString &suggestedName) = 0;
    // Abre un archivo con el programa que el sistema tenga para él.
    virtual void openDocument(const QString &path) = 0;

    // --- El formulario de alta ---
    // Lo abre como modal, con el combo de tipos ya lleno. No espera a que se
    // cierre: lo capturado llega después, con una señal de la vista.
    virtual void askNewExpense(const QList<application::ExpenseTypeOptionDto> &expenseTypes) = 0;
    // Mientras se guarda: Aceptar no se puede volver a presionar.
    virtual void setExpenseFormBusy(bool busy) = 0;
    // Marca bajo cada campo lo que reportó el dominio; el formulario sigue
    // abierto.
    virtual void showExpenseFormErrors(const QList<domain::ValidationError> &errors) = 0;
    virtual void closeExpenseForm() = 0;

protected:
    IFinanceView() = default;
    IFinanceView(const IFinanceView &) = default;
    IFinanceView &operator=(const IFinanceView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_FINANCE_IFINANCEVIEW_H
