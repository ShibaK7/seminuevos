#ifndef DOMAIN_COMMON_VALUE_OBJECTS_MONEYAMOUNT_H
#define DOMAIN_COMMON_VALUE_OBJECTS_MONEYAMOUNT_H

#include <QString>
#include <QtGlobal>

#include <optional>

namespace domain {

// Un importe capturado como texto que debe caber en una columna
// NUMERIC(12, 2): doce dígitos, dos de ellos decimales. Lo comparten el costo
// de un mantenimiento y el subtotal de un gasto, cada uno con sus mensajes.
//
// Solo dígitos y un punto decimal: ni signo (un importe negativo restaría de
// un total), ni comas, que en "1,500" serían separador de miles y en
// "1500,50" punto decimal. Se trabaja en centavos enteros para que el IVA y
// los totales no arrastren errores de punto flotante.
class MoneyAmount
{
public:
    static constexpr int kIntegerDigits = 10;
    static constexpr int kDecimals = 2;
    // 10^12 centavos: el primer valor que ya no cabe en NUMERIC(12, 2).
    static constexpr qint64 kCentsLimit = 1000000000000LL;

    enum class Problem {
        None,
        Empty,
        Negative,
        NotANumber,
        TooManyDecimals,
        TooManyIntegerDigits,
    };

    // Por qué el texto no cabe en la columna, o None si cabe. Ignora los
    // espacios alrededor.
    static Problem check(const QString &text);

    // El importe en centavos, o nullopt si check() reporta algún problema.
    static std::optional<qint64> toCents(const QString &text);

    // Centavos como se guardan y se muestran en un campo: "1500.50".
    static QString fromCents(qint64 cents);
};

} // namespace domain

#endif // DOMAIN_COMMON_VALUE_OBJECTS_MONEYAMOUNT_H
