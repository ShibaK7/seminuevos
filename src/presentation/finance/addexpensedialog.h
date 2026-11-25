#ifndef PRESENTATION_FINANCE_ADDEXPENSEDIALOG_H
#define PRESENTATION_FINANCE_ADDEXPENSEDIALOG_H

#include "application/finance/dto/financedtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QDialog>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui {
    class AddExpenseDialog;
}
QT_END_NAMESPACE

// El formulario modal de "Agregar Gasto": fecha (hoy de entrada), tipo (del
// catálogo operational_expense_cat, que trae su descripción), factura y su
// número, subtotal con el IVA y el total calculados, y método de pago. El
// folio no aparece: lo asigna la base al guardar. Su formulario es
// addexpensedialog.ui, en esta misma carpeta.
//
// Vista pasiva: Aceptar no cierra ni valida nada, solo entrega lo capturado
// (submitted). Quien guarda decide si cierra (accept()) o marca los errores
// que reportó el dominio (showErrors()). Cancelar sí lo cierra solo.
class AddExpenseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddExpenseDialog(const QList<application::ExpenseTypeOptionDto> &expenseTypes,
                              QWidget *parent = nullptr);
    ~AddExpenseDialog() override;

    // Lo capturado. Sin Factura, el número va vacío.
    application::NewOperationalExpenseDto expense() const;

    // Mientras se guarda, Aceptar no se puede volver a presionar.
    void setBusy(bool busy);
    // Marca cada campo con su error y le da el foco al primero. Un error sin
    // campo en el formulario se avisa aparte.
    void showErrors(const QList<domain::ValidationError> &errors);

signals:
    // Aceptar.
    void submitted(const application::NewOperationalExpenseDto &expense);

private:
    void showDescriptionOfSelectedType();
    void showAmounts();

    Ui::AddExpenseDialog *ui;
};

#endif // PRESENTATION_FINANCE_ADDEXPENSEDIALOG_H
