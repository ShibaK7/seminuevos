#include "domain/acquiredvehicle.h"

#include "domain/vehiclevisitor.h"

namespace domain {
namespace {

// Umbral legal para liquidar una operación en efectivo, expresado en veces
// la UMA diaria.
constexpr double kCashPaymentUmaMultiple = 3210.0;

} // namespace

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

std::unique_ptr<Vehicle> AcquiredVehicle::clone() const
{
    return std::make_unique<AcquiredVehicle>(*this);
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

double AcquiredVehicle::cashPaymentLimit() const
{
    return kCashPaymentUmaMultiple * m_umaDailyValue;
}

bool AcquiredVehicle::isCashPaymentAllowed() const
{
    // Sin UMA configurada no hay contra qué comparar. Se deja pasar en vez de
    // bloquear: un dato de configuración ausente no debería impedir capturar
    // una compra.
    if (m_umaDailyValue <= 0.0)
        return true;
    return m_purchasePrice < cashPaymentLimit();
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

    if (m_paymentType == PaymentType::Contado && !isCashPaymentAllowed()) {
        result.addError(
            QStringLiteral("paymentType"),
            QStringLiteral("El pago de contado no puede alcanzar las 3210 UMA ($%1). "
                           "Cambia la forma de pago a Crédito o ajusta el precio.")
                .arg(cashPaymentLimit(), 0, 'f', 2));
    }
}

} // namespace domain
