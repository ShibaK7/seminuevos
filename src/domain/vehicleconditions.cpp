#include "domain/vehicleconditions.h"

namespace domain {
namespace {

// Rango deliberadamente ancho: cubre desde una motocicleta monocilíndrica
// hasta un V16. El combo de la interfaz ofrece menos opciones, pero el
// dominio no tiene por qué ser más estrecho que la realidad.
constexpr int kMinCylinders = 1;
constexpr int kMaxCylinders = 16;

constexpr int kInteriorMaterialMaxLength = 50;

} // namespace

const CatalogRef &VehicleConditions::fuelType() const
{
    return m_fuelType;
}

void VehicleConditions::setFuelType(const CatalogRef &value)
{
    // Se acepta una referencia inválida sin chistar: el combo puede estar
    // vacío mientras el usuario captura. Que sea obligatoria al guardar lo
    // decide validate(), no el setter.
    m_fuelType = value;
}

int VehicleConditions::cylinders() const
{
    return m_cylinders;
}

bool VehicleConditions::setCylinders(int value)
{
    if (value < kMinCylinders || value > kMaxCylinders)
        return false;
    m_cylinders = value;
    return true;
}

Transmission VehicleConditions::transmission() const
{
    return m_transmission;
}

void VehicleConditions::setTransmission(Transmission value)
{
    m_transmission = value;
}

const QString &VehicleConditions::interiorMaterial() const
{
    return m_interiorMaterial;
}

bool VehicleConditions::setInteriorMaterial(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.length() > kInteriorMaterialMaxLength)
        return false;
    m_interiorMaterial = trimmed;
    return true;
}

WindowRegulators VehicleConditions::windowRegulators() const
{
    return m_windowRegulators;
}

void VehicleConditions::setWindowRegulators(WindowRegulators value)
{
    m_windowRegulators = value;
}

AirConditioning VehicleConditions::airConditioning() const
{
    return m_airConditioning;
}

void VehicleConditions::setAirConditioning(AirConditioning value)
{
    m_airConditioning = value;
}

ValidationResult VehicleConditions::validate() const
{
    ValidationResult result;

    if (!m_fuelType.isValid()) {
        result.addError(QStringLiteral("fuelType"),
                        QStringLiteral("Selecciona el tipo de combustible."));
    }
    // La columna es NOT NULL, así que un cero llegaría a la base como un
    // dato falso en vez de como un dato faltante.
    if (m_cylinders < kMinCylinders) {
        result.addError(QStringLiteral("cylinders"),
                        QStringLiteral("Indica el número de cilindros."));
    }

    return result;
}

} // namespace domain
