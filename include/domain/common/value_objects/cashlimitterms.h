#ifndef DOMAIN_COMMON_VALUE_OBJECTS_CASHLIMITTERMS_H
#define DOMAIN_COMMON_VALUE_OBJECTS_CASHLIMITTERMS_H

namespace domain {

// Los dos datos de configuración del tope de pago en efectivo: la UMA vigente
// y cuántas UMA son el tope. Los captura el Administrador en Configuraciones
// y la capa de aplicación los lee de global_configurations.
//
// Viaja junto, y no como un double suelto, porque la regla necesita los dos:
// con solo la UMA, el múltiplo quedaba fijo en el código.
struct CashLimitTerms
{
    // LFPIORPI, art. 32, fr. II: 3,210 UMA. El Administrador puede fijar un
    // tope más estricto, nunca uno mayor.
    static constexpr int kLegalUmaMultiple = 3210;

    // <= 0 significa "no configurada".
    double umaDailyValue = 0.0;
    int umaMultiple = kLegalUmaMultiple;

    bool hasUma() const { return umaDailyValue > 0.0; }
};

} // namespace domain

#endif // DOMAIN_COMMON_VALUE_OBJECTS_CASHLIMITTERMS_H
