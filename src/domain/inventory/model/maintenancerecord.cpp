#include "domain/inventory/model/maintenancerecord.h"

#include "domain/common/value_objects/moneyamount.h"

namespace domain {

namespace {

// Por qué el texto no cabe en NUMERIC(12, 2), o vacío si cabe. Un costo
// negativo restaría del total de mantenimientos de la unidad.
QString costProblem(const QString &cost)
{
    using Problem = MoneyAmount::Problem;
    switch (MoneyAmount::check(cost)) {
    case Problem::None:
        return QString();
    case Problem::Empty:
        return QStringLiteral("Captura el costo.");
    case Problem::Negative:
        return QStringLiteral("El costo no puede ser negativo.");
    case Problem::NotANumber:
        return QStringLiteral("El costo debe ser un número, por ejemplo 1500.50.");
    case Problem::TooManyDecimals:
        return QStringLiteral("El costo admite hasta %1 decimales.")
            .arg(MaintenanceRecord::kCostDecimals);
    case Problem::TooManyIntegerDigits:
        return QStringLiteral("El costo admite hasta %1 dígitos antes del punto.")
            .arg(MaintenanceRecord::kCostIntegerDigits);
    }
    return QString();
}

} // namespace

MaintenanceRecord::MaintenanceRecord(int vehicleFolio, const QDate &date, int typeId,
                                     const QString &cost,
                                     std::optional<PaymentMethod> paymentMethod,
                                     const QString &description)
    : m_vehicleFolio(vehicleFolio)
    , m_date(date)
    , m_typeId(typeId)
    , m_cost(cost.trimmed())
    , m_paymentMethod(paymentMethod)
    , m_description(description.trimmed())
{
}

int MaintenanceRecord::vehicleFolio() const
{
    return m_vehicleFolio;
}

const QDate &MaintenanceRecord::date() const
{
    return m_date;
}

int MaintenanceRecord::typeId() const
{
    return m_typeId;
}

const QString &MaintenanceRecord::cost() const
{
    return m_cost;
}

std::optional<PaymentMethod> MaintenanceRecord::paymentMethod() const
{
    return m_paymentMethod;
}

const QString &MaintenanceRecord::description() const
{
    return m_description;
}

ValidationResult MaintenanceRecord::validate() const
{
    ValidationResult result;
    if (m_vehicleFolio <= 0)
        result.addError(QStringLiteral("folio"),
                        QStringLiteral("La unidad todavía no está guardada."));
    if (!m_date.isValid())
        result.addError(QStringLiteral("maintenanceDate"),
                        QStringLiteral("Elige la fecha del mantenimiento."));
    // Los ids de vehicle_maintenance_cat salen de un SERIAL: empiezan en 1.
    if (m_typeId <= 0)
        result.addError(QStringLiteral("maintenanceType"),
                        QStringLiteral("Selecciona el tipo de mantenimiento."));
    const QString problem = costProblem(m_cost);
    if (!problem.isEmpty())
        result.addError(QStringLiteral("cost"), problem);
    // payment_method es NOT NULL: no hay un valor por omisión que suponer.
    if (!m_paymentMethod)
        result.addError(QStringLiteral("paymentMethod"),
                        QStringLiteral("Selecciona el método de pago."));
    result.merge(validateDescription(m_description));
    return result;
}

ValidationResult MaintenanceRecord::validateDescription(const QString &description)
{
    ValidationResult result;
    // Es lo que dice el recibo después de "el cual se describe a
    // continuación": sin ella el comprobante queda incompleto.
    if (description.trimmed().isEmpty())
        result.addError(QStringLiteral("description"),
                        QStringLiteral("Captura la descripción del mantenimiento."));
    return result;
}

} // namespace domain
