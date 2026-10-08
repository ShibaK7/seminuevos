#ifndef DOMAIN_RULES_CASHPAYMENTLIMIT_H
#define DOMAIN_RULES_CASHPAYMENTLIMIT_H

#include "domain/value_objects/enums.h"
#include "domain/value_objects/validationresult.h"

#include <QString>

namespace domain {

// Tope legal para pagar un vehículo en EFECTIVO: la LFPIORPI (art. 32,
// fr. II) prohíbe liquidar en monedas y billetes cualquier operación que
// transmita la propiedad de un vehículo por 3,210 UMA o más.
//
// Es una regla aparte, y no un método de AcquiredVehicle, porque no le
// pertenece a una sola entidad: aplica igual a la compra que hace la agencia
// que a la venta que hará después. Cuando exista el flujo de ventas, la usará
// tal cual.
//
// La restricción es sobre el MÉTODO de pago (efectivo vs. transferencia), no
// sobre el tipo (contado vs. crédito): un pago de contado por transferencia es
// legal a cualquier monto.
//
// Sin UMA configurada no hay contra qué comparar. En ese caso se rechaza el
// efectivo en vez de dejarlo pasar: dejarlo pasar significaría aceptar
// cualquier monto en efectivo solo porque falta un dato de configuración.
class CashPaymentLimit
{
public:
    static constexpr double kUmaMultiple = 3210.0;

    // umaDailyValue <= 0 significa "no configurada".
    explicit CashPaymentLimit(double umaDailyValue);

    bool isConfigured() const;
    // 0.0 si la UMA no está configurada.
    double limit() const;

    // true si el pago se puede hacer con ese método por ese monto. Todo lo
    // que no es efectivo está permitido.
    bool allows(PaymentMethod method, double amount) const;

    // Agrega a `result` el error correspondiente si el pago no está permitido.
    void check(PaymentMethod method, double amount, ValidationResult &result,
               const QString &field = QStringLiteral("paymentMethod")) const;

private:
    double m_umaDailyValue;
};

} // namespace domain

#endif // DOMAIN_RULES_CASHPAYMENTLIMIT_H
