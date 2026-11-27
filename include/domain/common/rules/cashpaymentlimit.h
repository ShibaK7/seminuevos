#ifndef DOMAIN_COMMON_RULES_CASHPAYMENTLIMIT_H
#define DOMAIN_COMMON_RULES_CASHPAYMENTLIMIT_H

#include "domain/common/value_objects/cashlimitterms.h"
#include "domain/common/value_objects/enums.h"
#include "domain/common/value_objects/validationresult.h"

#include <QString>

namespace domain {

// Tope legal para pagar un vehículo en EFECTIVO: la LFPIORPI (art. 32,
// fr. II) prohíbe liquidar en monedas y billetes cualquier operación que
// transmita la propiedad de un vehículo por 3,210 UMA o más.
//
// Es una regla aparte, y no un método de AcquiredVehicle, porque no le
// pertenece a una sola entidad: aplica igual a la compra que hace la agencia
// que a la venta que hará después.
//
// La restricción es sobre el MÉTODO de pago (efectivo vs. transferencia), no
// sobre el tipo (contado vs. crédito): un pago de contado por transferencia es
// legal a cualquier monto.
//
// El tope en UMA lo fija el Administrador en Configuraciones, y puede ser más
// estricto que el de la ley pero nunca mayor: un múltiplo arriba de 3,210 (o
// que no sea positivo) se toma como el legal, para que un dato mal capturado
// no deje pasar un pago que la ley prohíbe.
//
// Sin UMA configurada no hay contra qué comparar. En ese caso se rechaza el
// efectivo en vez de dejarlo pasar: dejarlo pasar significaría aceptar
// cualquier monto en efectivo solo porque falta un dato de configuración.
class CashPaymentLimit
{
public:
    explicit CashPaymentLimit(const CashLimitTerms &terms);
    // Con el tope legal (3,210 UMA).
    explicit CashPaymentLimit(double umaDailyValue);

    bool isConfigured() const;
    // El múltiplo con que se calcula el tope, ya acotado al legal.
    int umaMultiple() const;
    // 0.0 si la UMA no está configurada.
    double limit() const;

    // true si el pago se puede hacer con ese método por ese monto. Todo lo
    // que no es efectivo está permitido.
    bool allows(PaymentMethod method, double amount) const;

    // Agrega a `result` el error correspondiente si el pago no está permitido.
    void check(PaymentMethod method, double amount, ValidationResult &result,
               const QString &field = QStringLiteral("paymentMethod")) const;

private:
    // El tope en centavos: se compara en enteros, no en double.
    qint64 limitCents() const;

    double m_umaDailyValue;
    int m_umaMultiple;
};

} // namespace domain

#endif // DOMAIN_COMMON_RULES_CASHPAYMENTLIMIT_H
