#ifndef DOMAIN_ACQUIREDVEHICLE_H
#define DOMAIN_ACQUIREDVEHICLE_H

#include "domain/vehicle.h"

namespace domain {

// Unidad que la agencia COMPRÓ. Corresponde a la subtabla
// vehicle_acquisitions.
//
// `final` no es adorno: cierra la jerarquía en dos ramas, que es lo que el
// esquema modela, y permite al compilador resolver las llamadas virtuales
// cuando conoce el tipo concreto.
class AcquiredVehicle final : public Vehicle
{
public:
    AcquiredVehicle() = default;

    // --- Interfaz polimórfica --------------------------------------------
    AcquisitionType acquisitionType() const override;
    QString counterpartyRole() const override;
    // Precio capturado: aquí la agencia decide en cuánto revende.
    double salePrice() const override;
    // Lo que costó tener la unidad lista para vender.
    double totalCost() const override;
    double expectedProfit() const override;
    bool acceptsInvoiceType(InvoiceType type) const override;
    void accept(VehicleVisitor &visitor) const override;
    std::unique_ptr<Vehicle> clone() const override;

    // --- Datos propios de la compra ---------------------------------------
    double purchasePrice() const;
    [[nodiscard]] bool setPurchasePrice(double value);

    [[nodiscard]] bool setSalePrice(double value);

    PaymentType paymentType() const;
    void setPaymentType(PaymentType value);

    PaymentMethod paymentMethod() const;
    void setPaymentMethod(PaymentMethod value);

    const QString &invoiceFilePath() const;
    void setInvoiceFilePath(const QString &value);

    // --- Regla del pago en efectivo ---------------------------------------
    // La UMA vigente a la fecha de la operación es un dato del trato, no algo
    // que el dominio salga a consultar: la inyecta la capa de aplicación
    // desde global_configurations. Así esta clase nunca toca la base de datos
    // y la regla se puede comprobar sin levantar PostgreSQL.
    double umaDailyValue() const;
    [[nodiscard]] bool setUmaDailyValue(double value);

    // Tope legal para liquidar en efectivo. Devuelve 0.0 si no se inyectó la
    // UMA, caso en el que la regla no se puede evaluar y no se aplica.
    double cashPaymentLimit() const;
    bool isCashPaymentAllowed() const;

    // La agencia PUEDE rematar a pérdida a propósito, así que esto es una
    // consulta para avisar, no un error que impida guardar. Convertirlo en
    // invariante bloquearía una operación legítima.
    bool isSoldAtLoss() const;

protected:
    void collectSpecificErrors(ValidationResult &result) const override;

private:
    double m_purchasePrice = 0.0;
    double m_salePrice = 0.0;
    PaymentType m_paymentType = PaymentType::Contado;
    PaymentMethod m_paymentMethod = PaymentMethod::Efectivo;
    QString m_invoiceFilePath;
    double m_umaDailyValue = 0.0;
};

} // namespace domain

#endif // DOMAIN_ACQUIREDVEHICLE_H
