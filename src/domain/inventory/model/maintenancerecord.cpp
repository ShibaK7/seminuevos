#include "domain/inventory/model/maintenancerecord.h"

#include <QLatin1Char>

namespace domain {

namespace {

// Por qué el texto no cabe en NUMERIC(12, 2), o vacío si cabe. Solo dígitos
// y un punto decimal: ni signo (un costo negativo restaría del total de
// mantenimientos de la unidad), ni comas, que en "1,500" serían separador de
// miles y en "1500,50" punto decimal.
QString costProblem(const QString &cost)
{
    if (cost.isEmpty())
        return QStringLiteral("Captura el costo.");
    if (cost.startsWith(QLatin1Char('-')))
        return QStringLiteral("El costo no puede ser negativo.");

    const qsizetype point = cost.indexOf(QLatin1Char('.'));
    const QString integerPart = point < 0 ? cost : cost.left(point);
    const QString decimalPart = point < 0 ? QString() : cost.mid(point + 1);
    const auto allDigits = [](const QString &text) {
        for (const QChar c : text) {
            if (c < QLatin1Char('0') || c > QLatin1Char('9'))
                return false;
        }
        return true;
    };
    if (integerPart.isEmpty() || !allDigits(integerPart) || !allDigits(decimalPart)
        || (point >= 0 && decimalPart.isEmpty()))
        return QStringLiteral("El costo debe ser un número, por ejemplo 1500.50.");
    if (decimalPart.size() > MaintenanceRecord::kCostDecimals)
        return QStringLiteral("El costo admite hasta %1 decimales.")
            .arg(MaintenanceRecord::kCostDecimals);

    // Los ceros a la izquierda no cuentan contra el tope: "000123" es 123.
    qsizetype significant = 0;
    while (significant < integerPart.size() - 1 && integerPart.at(significant) == QLatin1Char('0'))
        ++significant;
    if (integerPart.size() - significant > MaintenanceRecord::kCostIntegerDigits)
        return QStringLiteral("El costo admite hasta %1 dígitos antes del punto.")
            .arg(MaintenanceRecord::kCostIntegerDigits);
    return QString();
}

} // namespace

MaintenanceRecord::MaintenanceRecord(int vehicleFolio, const QDate &date, int typeId,
                                     const QString &cost, const QString &description)
    : m_vehicleFolio(vehicleFolio)
    , m_date(date)
    , m_typeId(typeId)
    , m_cost(cost.trimmed())
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
