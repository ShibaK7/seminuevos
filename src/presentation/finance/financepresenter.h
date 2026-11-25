#ifndef PRESENTATION_FINANCE_FINANCEPRESENTER_H
#define PRESENTATION_FINANCE_FINANCEPRESENTER_H

#include "application/finance/dto/financedtos.h"
#include "domain/common/value_objects/enums.h"

#include <QList>
#include <QObject>

namespace application {
class OperationalExpenseService;
}

namespace presentation {

class IFinanceView;
class TaskRunner;

// Presenter de la página de Finanzas (Gastos Generales): pinta la tabla de
// gastos, abre "Agregar Gasto" con su catálogo, guarda lo capturado, corrige
// la factura de un renglón desde la tabla e imprime su recibo de pago. Todo lo que toca la base corre
// fuera del hilo de la interfaz. A quien su rol no le deja usar Finanzas no
// se le pinta nada (el menú ni siquiera se ve).
//
// Sin widgets (solo QtCore): se prueba con una vista falsa.
class FinancePresenter final : public QObject
{
    Q_OBJECT

public:
    FinancePresenter(IFinanceView &view, const application::OperationalExpenseService &service,
                     TaskRunner &runner, domain::UserRole role, QObject *parent = nullptr);

    // Lee y pinta los gastos del filtro actual, los años del combo y la
    // tarjeta "Total Egresos". Corre también después de cada alta: así el año
    // de un gasto nuevo aparece sin compararlo.
    void reload();
    // Se cambió Periodo o Año: vuelve a leer con el filtro nuevo.
    void onFilterChanged(const application::OperationalExpenseFilterDto &filter);

    // "Agregar Gasto". Mientras se lee el catálogo, otro clic no abre un
    // segundo formulario.
    void onAddExpense();
    // Aceptar en el formulario: guarda, cierra y repinta la tabla con el
    // folio que asignó la base; o deja el formulario abierto con los errores.
    void onSubmitExpense(const application::NewOperationalExpenseDto &expense);
    // Se marcó o desmarcó la casilla Factura de un renglón. Primero pregunta
    // si se hace el cambio; al cancelar, la tabla vuelve a lo guardado. Al
    // desmarcarla se guarda sin factura; al marcarla, si falta el número se
    // abre su celda y se guarda cuando se capture.
    void onInvoiceToggled(int folio, bool hasInvoice);
    // Se cerró la celda No. Factura de un renglón con su Factura marcada
    // (llega aunque el texto no haya cambiado). Si quedó vacía tras marcar la
    // casilla, alerta que falta el número. Si reemplaza un número guardado,
    // primero pregunta. Al cancelar, o si no se pudo guardar, la tabla vuelve
    // a lo guardado.
    void onInvoiceNumberEdited(int folio, const QString &invoiceNumber);
    // "Imprimir": el recibo de pago del renglón seleccionado, con los datos
    // tal como se leyeron de la base (no el texto de la tabla).
    void onPrintReceipt();

private:
    void saveInvoice(int folio, bool hasInvoice, const QString &invoiceNumber);
    const application::OperationalExpenseItemDto *find(int folio) const;

    IFinanceView &m_view;
    const application::OperationalExpenseService &m_service;
    TaskRunner &m_runner;
    const domain::UserRole m_role;
    bool m_loadingForm = false;
    bool m_saving = false;
    bool m_updatingInvoice = false;
    // Cada lectura de la tabla lleva un número; solo se pinta la última.
    int m_listGeneration = 0;
    // El filtro de la tabla; empieza en "Todo", igual que los combos.
    application::OperationalExpenseFilterDto m_filter;
    // Lo último que se pintó, para comparar lo que edita el usuario.
    QList<application::OperationalExpenseItemDto> m_items;
};

} // namespace presentation

#endif // PRESENTATION_FINANCE_FINANCEPRESENTER_H
