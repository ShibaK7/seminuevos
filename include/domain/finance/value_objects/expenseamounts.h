#ifndef DOMAIN_FINANCE_VALUE_OBJECTS_EXPENSEAMOUNTS_H
#define DOMAIN_FINANCE_VALUE_OBJECTS_EXPENSEAMOUNTS_H

#include <QString>
#include <QtGlobal>

#include <optional>

namespace domain {

// Subtotal, IVA y total de un gasto operativo, en centavos. El IVA es el 16 %
// del subtotal, redondeado al centavo (la mitad sube), y el total es la suma
// de los dos. Es el único lugar que hace esa cuenta: el formulario de alta la
// usa para mostrarla mientras se captura y OperationalExpense para guardarla.
class ExpenseAmounts
{
public:
    static constexpr int kIvaPercent = 16;

    // Las cantidades de un subtotal capturado, o nullopt si el texto no es un
    // importe que cabe en NUMERIC(12, 2) (ver MoneyAmount).
    static std::optional<ExpenseAmounts> fromSubtotal(const QString &subtotal);

    qint64 subtotalCents() const;
    qint64 ivaCents() const;
    qint64 totalCents() const;

    // Como se guardan y se muestran en un campo: "1500.50".
    QString subtotal() const;
    QString iva() const;
    QString total() const;

private:
    explicit ExpenseAmounts(qint64 subtotalCents);

    qint64 m_subtotalCents;
    qint64 m_ivaCents;
};

} // namespace domain

#endif // DOMAIN_FINANCE_VALUE_OBJECTS_EXPENSEAMOUNTS_H
