#ifndef DOMAIN_FINANCE_RULES_FINANCEACCESS_H
#define DOMAIN_FINANCE_RULES_FINANCEACCESS_H

#include "domain/common/value_objects/enums.h"

namespace domain {

// Qué puede hacer cada rol en el módulo de Finanzas. Es el ÚNICO lugar que lo
// decide: el menú lateral y el servicio de gastos le preguntan a esta clase,
// y nadie más compara roles.
//
// Es una regla aparte, y no un método de OperationalExpense, porque no le
// pertenece al gasto: depende de quién lo consulta.
class FinanceAccess
{
public:
    explicit FinanceAccess(UserRole role);

    // Ver el módulo, leer los gastos, darlos de alta y corregirlos. Hoy solo
    // el Administrador.
    bool canUseFinance() const;

private:
    UserRole m_role;
};

} // namespace domain

#endif // DOMAIN_FINANCE_RULES_FINANCEACCESS_H
