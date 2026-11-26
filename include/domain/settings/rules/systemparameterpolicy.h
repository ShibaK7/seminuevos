#ifndef DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H
#define DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H

#include "domain/common/value_objects/validationresult.h"
#include "domain/settings/value_objects/systemparameter.h"

namespace domain {

// Qué valor puede tomar cada parámetro de Configuraciones. Se guardan en
// NUMERIC(12, 2), así que ninguno pasa de 9,999,999,999.99. Además:
//   - la UMA es mayor que cero: con cero el efectivo se rechazaría siempre;
//   - el tope de efectivo es un número entero de UMA, de 1 a 3,210: se puede
//     fijar uno más estricto que la ley, nunca uno mayor;
//   - los saldos no son negativos.
class SystemParameterPolicy
{
public:
    // El mayor valor que cabe en NUMERIC(12, 2), en centésimos.
    static constexpr qint64 kMaxHundredths = Q_INT64_C(999999999999);

    // El valor va en centésimos. Agrega a `result` el error, en el campo del
    // parámetro, si no se puede guardar.
    static void check(SystemParameter parameter, qint64 hundredths, ValidationResult &result);
};

} // namespace domain

#endif // DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H
