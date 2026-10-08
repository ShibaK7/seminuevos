#ifndef DOMAIN_MODEL_CONSIGNEDVEHICLE_H
#define DOMAIN_MODEL_CONSIGNEDVEHICLE_H

#include "domain/model/vehicle.h"

namespace domain {

// Unidad que la agencia vende POR CUENTA DE SU DUEÑO, cobrando una comisión.
// Corresponde a la subtabla vehicle_consignments.
//
// La agencia nunca compra el auto: el precio base se le entrega íntegro al
// propietario cuando se vende, y lo que gana la agencia es la comisión. Esa
// diferencia con la compraventa es la que hace que totalCost() y
// expectedProfit() tengan que ser virtuales y no una fórmula común.
class ConsignedVehicle final : public Vehicle
{
public:
    ConsignedVehicle() = default;

    // --- Interfaz polimórfica --------------------------------------------
    AcquisitionType acquisitionType() const override;
    QString counterpartyRole() const override;

    // CALCULADO, no capturado: precio base más la comisión. Espeja la columna
    // generada de vehicle_consignments, y por eso no existe un setSalePrice()
    // en esta clase. Ese método ausente ES el encapsulamiento: un precio
    // derivado no se puede contradecir desde fuera.
    double salePrice() const override;

    // Solo el mantenimiento. La unidad nunca se compró, así que no hay precio
    // de compra que sumar.
    double totalCost() const override;

    // Comisión menos mantenimiento. Ojo: NO es salePrice() - totalCost(),
    // porque el precio base no es ganancia de la agencia sino dinero del
    // dueño.
    double expectedProfit() const override;

    bool acceptsInvoiceType(InvoiceType type) const override;
    void accept(VehicleVisitor &visitor) const override;
    std::unique_ptr<Vehicle> clone() const override;

    // Mismo documento que la compra, con la misma información: lo único que
    // cambia es cómo se titula la operación.
    bool canGenerateContract() const override;
    QString contractTitle() const override;
    // El precio base, o sea lo que se le va a entregar al propietario. NO el
    // precio de venta, que incluye la comisión de la agencia y es lo que
    // acabará pagando un tercero.
    double contractAmount() const override;
    QMap<QString, QString> contractPlaceholders() const override;

    // --- Datos propios de la consignación ---------------------------------
    // Lo que el dueño quiere recibir por su unidad.
    double basePrice() const;
    [[nodiscard]] bool setBasePrice(double value);

    // Porcentaje que cobra la agencia, de 0 a 100.
    double commissionRate() const;
    [[nodiscard]] bool setCommissionRate(double value);

    // Lo que gana la agencia si se vende al precio calculado.
    double commissionAmount() const;

    // Lo que se le entrega al dueño: el precio base íntegro.
    double ownerPayout() const;

protected:
    void collectSpecificErrors(ValidationResult &result) const override;

private:
    double m_basePrice = 0.0;
    double m_commissionRate = 0.0;
};

} // namespace domain

#endif // DOMAIN_MODEL_CONSIGNEDVEHICLE_H
