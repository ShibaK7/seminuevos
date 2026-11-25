#ifndef DOMAIN_FINANCE_VALUE_OBJECTS_EXPENSETREND_H
#define DOMAIN_FINANCE_VALUE_OBJECTS_EXPENSETREND_H

#include <optional>

namespace domain {

// Cuánto cambiaron los egresos de un mes contra el anterior, en porcentaje:
// positivo si gastó más, negativo si gastó menos. Es el único lugar que hace
// la cuenta; la tarjeta "Total Egresos" solo la pinta.
class ExpenseTrend
{
public:
    // (actual - anterior) / anterior * 100. Sin gastos en ningún mes es 0.
    // Si el mes anterior no tuvo gastos y este sí, no hay base para un
    // porcentaje: nullopt.
    static std::optional<double> percentChange(double current, double previous);
};

} // namespace domain

#endif // DOMAIN_FINANCE_VALUE_OBJECTS_EXPENSETREND_H
