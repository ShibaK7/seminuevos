#ifndef DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H
#define DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H

#include "domain/common/value_objects/validationresult.h"
#include "domain/settings/value_objects/systemparameter.h"

#include <optional>

namespace domain {

// Qué valor puede tomar cada parámetro de Configuraciones. Se guardan en
// NUMERIC(12, 2), así que ninguno pasa de 9,999,999,999.99. Además:
//   - la UMA es mayor que cero (con cero el efectivo se rechazaría siempre)
//     y no puede ser más del doble ni menos de la mitad de la vigente:
//     multiplica el tope de efectivo, y un punto decimal fuera de lugar (11731
//     por 117.31) dejaría pasar cualquier pago en efectivo. La UMA sube 4-5 %
//     al año, así que la regla no caduca como un máximo fijo. La de 2026 es
//     de $117.31. Si nunca se capturó, basta con que sea mayor que cero;
//   - el tope de efectivo es un número entero de UMA, de 1 a 3,210: se puede
//     fijar uno más estricto que la ley, nunca uno mayor.
class SystemParameterPolicy
{
public:
    // El mayor valor que cabe en NUMERIC(12, 2), en centésimos.
    static constexpr qint64 kMaxHundredths = Q_INT64_C(999999999999);
    // Cuántas veces puede subir (o bajar) la UMA de un solo cambio.
    static constexpr qint64 kMaxUmaJump = 2;

    // Los valores van en centésimos; `current` es el guardado (ninguno si
    // nunca se capturó). Agrega a `result` el error, en el campo del
    // parámetro, si no se puede guardar.
    static void check(SystemParameter parameter, qint64 hundredths, std::optional<qint64> current,
                      ValidationResult &result);
};

} // namespace domain

#endif // DOMAIN_SETTINGS_RULES_SYSTEMPARAMETERPOLICY_H
