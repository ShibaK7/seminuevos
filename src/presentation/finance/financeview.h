#ifndef PRESENTATION_FINANCE_FINANCEVIEW_H
#define PRESENTATION_FINANCE_FINANCEVIEW_H

#include "presentation/finance/ifinanceview.h"

#include <QPointer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class FinanceView;
}
QT_END_NAMESPACE

namespace presentation {
class FinancePresenter;
}

class AddExpenseDialog;

// La página de Finanzas ("Gastos Generales"): filtros de periodo y año, las
// tarjetas de resumen (Total Egresos del mes, Banco y Caja Chica) y la tabla
// de gastos registrados con "Agregar Gasto" e "Imprimir" (el recibo de pago
// del renglón seleccionado). Su formulario es financeview.ui, en esta misma
// carpeta.
//
// Vista pasiva de FinancePresenter (IFinanceView): pinta los gastos que le da
// el presenter, abre el formulario de alta (AddExpenseDialog) y avisa lo que
// el usuario cambia en la tabla y en los filtros de Periodo y Año (los dos
// con "Todo"). Las tarjetas Banco y Caja Chica todavía no hacen nada.
//
// En la tabla solo se editan dos cosas: la casilla Factura (con un clic) y el
// No. Factura (con doble clic), este último solo si su Factura está marcada;
// sin ella, la celda dice "Sin Factura". Al cambiarlas avisa invoiceToggled o
// invoiceNumberEdited y el presenter decide si se guarda.
class FinanceView : public QWidget, public presentation::IFinanceView
{
    Q_OBJECT

public:
    // El orden de las columnas en el .ui. Solo nombra lo que está en Designer
    // para no repetir números sueltos.
    enum Column {
        FolioColumn,
        DateColumn,
        ExpenseTypeColumn,
        DescriptionColumn,
        InvoiceColumn,
        InvoiceNumberColumn,
        SubtotalColumn,
        IvaColumn,
        TotalColumn,
        PaymentMethodColumn,
    };

    explicit FinanceView(QWidget *parent = nullptr);
    ~FinanceView() override;

    // Conecta los botones y la tabla con el presenter.
    void bind(presentation::FinancePresenter &presenter);

    // --- IFinanceView ---
    void setBusy(bool busy) override;
    void showExpenses(const QList<application::OperationalExpenseItemDto> &items) override;
    void showWarning(const QString &title, const QString &message) override;
    void showYears(const QList<int> &years) override;
    void showMonthlySummary(const application::MonthlyExpenseSummaryDto &summary) override;

    // Lo elegido en Periodo y Año.
    application::OperationalExpenseFilterDto filter() const;
    void editInvoiceNumber(int folio) override;
    bool confirmInvoiceUpdate() override;
    bool confirmInvoiceNumberUpdate() override;
    bool askForMissingInvoiceNumber() override;
    int selectedExpenseFolio() override;
    QString askReceiptPath(const QString &suggestedName) override;
    void openDocument(const QString &path) override;
    void askNewExpense(const QList<application::ExpenseTypeOptionDto> &expenseTypes) override;
    void setExpenseFormBusy(bool busy) override;
    void showExpenseFormErrors(const QList<domain::ValidationError> &errors) override;
    void closeExpenseForm() override;

signals:
    // "Agregar Gasto".
    void addExpenseRequested();
    // "Imprimir": el recibo de pago del renglón seleccionado.
    void printReceiptRequested();
    // Se cambió Periodo o Año.
    void filterChanged(const application::OperationalExpenseFilterDto &filter);
    // Aceptar en el formulario de alta, con lo capturado.
    void expenseSubmitted(const application::NewOperationalExpenseDto &expense);
    // Se marcó o desmarcó la casilla Factura de un renglón.
    void invoiceToggled(int folio, bool hasInvoice);
    // Se cerró la celda No. Factura de un renglón con su Factura marcada, con
    // Enter, al salir de ella o con Esc; aunque el texto no haya cambiado.
    void invoiceNumberEdited(int folio, const QString &invoiceNumber);

private:
    int rowOf(int folio) const;
    // Pregunta con Cancelar y Aceptar; true si aceptó.
    bool confirm(const QString &title, const QString &question, const QString &detail);
    // El No. Factura se edita solo mientras su Factura está marcada.
    void syncInvoiceNumberEditable(int row);

    Ui::FinanceView *ui;
    // El formulario de alta abierto, si hay uno. Se borra solo al cerrarse.
    QPointer<AddExpenseDialog> m_form;
    // Mientras la vista pinta, los cambios de las celdas no son del usuario.
    bool m_painting = false;
};

#endif // PRESENTATION_FINANCE_FINANCEVIEW_H
