#ifndef DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H
#define DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H

#include "domain/common/value_objects/validationresult.h"
#include "domain/settings/value_objects/systemparameter.h"

namespace domain {

// Qué valor puede tomar cada parámetro de Configuraciones. Se guardan en
// NUMERIC(12, 2), así que ninguno pasa de 9,999,999,999.99. Además:
//   - la UMA es mayor que cero (con cero el efectivo se rechazaría siempre)
//     y no pasa de $1,000: multiplica el tope de efectivo, y un punto decimal
//     fuera de lugar (11731 por 117.31) dejaría pasar cualquier pago en
//     efectivo. La de 2026 es de $117.31;
//   - el tope de efectivo es un número entero de UMA, de 1 a 3,210: se puede
//     fijar uno más estricto que la ley, nunca uno mayor.
class SystemParameterPolicy
{
public:
    // El mayor valor que cabe en NUMERIC(12, 2), en centésimos.
    static constexpr qint64 kMaxHundredths = Q_INT64_C(999999999999);
    // La mayor UMA diaria que se acepta, en centésimos: $1,000.00.
    static constexpr qint64 kMaxUmaHundredths = 100000;

    // El valor va en centésimos. Agrega a `result` el error, en el campo del
    // parámetro, si no se puede guardar.
    static void check(SystemParameter parameter, qint64 hundredths, ValidationResult &result);
};

} // namespace domain

#endif // DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H
