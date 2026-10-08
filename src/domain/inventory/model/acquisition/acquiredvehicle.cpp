#include "domain/inventory/model/acquisition/acquiredvehicle.h"

#include "domain/common/rules/cashpaymentlimit.h"
#include "domain/inventory/model/vehiclevisitor.h"

namespace domain {

AcquisitionType AcquiredVehicle::acquisitionType() const
{
    return AcquisitionType::Adquisicion;
}

QString AcquiredVehicle::counterpartyRole() const
{
    return QStringLiteral("Vendedor");
}

double AcquiredVehicle::salePrice() const
{
    return m_salePrice;
}

double AcquiredVehicle::totalCost() const
{
    return m_purchasePrice + maintenanceCost();
}

double AcquiredVehicle::expectedProfit() const
{
    return salePrice() - totalCost();
}

bool AcquiredVehicle::acceptsInvoiceType(InvoiceType type) const
{
    return type == InvoiceType::Facturado || type == InvoiceType::Autofactura;
}

// Definido fuera de línea, no en el header: aquí sí se puede incluir
// vehiclevisitor.h sin crear un ciclo con vehicle.h.
void AcquiredVehicle::accept(VehicleVisitor &visitor) const
{
    visitor.visit(*this);
}

double AcquiredVehicle::purchasePrice() const
{
    return m_purchasePrice;
}

bool AcquiredVehicle::setPurchasePrice(double value)
{
    if (value <= 0.0)
        return false;
    m_purchasePrice = value;
    return true;
}

bool AcquiredVehicle::setSalePrice(double value)
{
    if (value < 0.0)
        return false;
    m_salePrice = value;
    return true;
}

PaymentType AcquiredVehicle::paymentType() const
{
    return m_paymentType;
}

void AcquiredVehicle::setPaymentType(PaymentType value)
{
    m_paymentType = value;
}

PaymentMethod AcquiredVehicle::paymentMethod() const
{
    return m_paymentMethod;
}

void AcquiredVehicle::setPaymentMethod(PaymentMethod value)
{
    m_paymentMethod = value;
}

const QString &AcquiredVehicle::invoiceFilePath() const
{
    return m_invoiceFilePath;
}

void AcquiredVehicle::setInvoiceFilePath(const QString &value)
{
    // También la usa el worker para sustituir la ruta absoluta por la
    // definitiva dentro del almacén, después de copiar el archivo.
    m_invoiceFilePath = value;
}

double AcquiredVehicle::umaDailyValue() const
{
    return m_umaDailyValue;
}

bool AcquiredVehicle::setUmaDailyValue(double value)
{
    if (value < 0.0)
        return false;
    m_umaDailyValue = value;
    return true;
}

bool AcquiredVehicle::isSoldAtLoss() const
{
    return m_salePrice < totalCost();
}

bool AcquiredVehicle::canGenerateContract() const
{
    return true;
}

QString AcquiredVehicle::contractTitle() const
{
    return QStringLiteral("COMPRA - VENTA AUTOMOTRIZ");
}

double AcquiredVehicle::contractAmount() const
{
    // El contrato declara lo que la agencia le paga al vendedor, no lo que
    // después pedirá por la unidad.
    return m_purchasePrice;
}

QMap<QString, QString> AcquiredVehicle::contractPlaceholders() const
{
    QMap<QString, QString> values = Vehicle::contractPlaceholders();

    values.insert(QStringLiteral("condiciones_pago"),
                  m_paymentType == PaymentType::Contado
                      ? QStringLiteral("PAGO DE CONTADO EN UNA SOLA EXHIBICIÓN.")
                      : QStringLiteral("PAGO A CRÉDITO SEGÚN LAS CONDICIONES ACORDADAS ENTRE "
                                       "LAS PARTES."));

    return values;
}

void AcquiredVehicle::collectSpecificErrors(ValidationResult &result) const
{
    if (m_purchasePrice <= 0.0) {
        result.addError(QStringLiteral("purchasePrice"),
                        QStringLiteral("Captura el precio de compra."));
    }

    // Precio de venta obligatorio: la pantalla lo marca como requerido, y una
    // unidad en inventario sin precio no se puede ofrecer. Cero no es un
    // precio, es un dato que falta.
    if (m_salePrice <= 0.0) {
        result.addError(QStringLiteral("salePrice"),
                        QStringLiteral("Captura el precio de venta."));
    }

    // El tope legal es sobre el MÉTODO de pago (efectivo), no sobre el tipo
    // (contado/crédito): un contado por transferencia es legal a cualquier
    // monto. Lo que la agencia liquida es el precio de compra.
    CashPaymentLimit(m_umaDailyValue).check(m_paymentMethod, m_purchasePrice, result);
}

} // namespace domain
