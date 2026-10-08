#ifndef DOMAIN_MODEL_VEHICLECONDITIONS_H
#define DOMAIN_MODEL_VEHICLECONDITIONS_H

#include "domain/value_objects/catalogref.h"
#include "domain/value_objects/enums.h"
#include "domain/value_objects/validationresult.h"

#include <QString>

#include <optional>

namespace domain {

// Especificaciones técnicas de la unidad: el panel izquierdo del Paso 2 del
// wizard. Mapea vehicle_conditions uno a uno.
//
// Va separada de Inspection a propósito, aunque las dos salgan de la misma
// pantalla: son tablas distintas y, sobre todo, se validan en momentos
// distintos. Tenerlas aparte permite validar el Paso 1 sin que salten
// errores de "faltan los cilindros", sin necesidad de banderas de alcance.
//
// Transmisión, cristales y aire acondicionado sí pueden faltar: "sin elegir"
// es un estado real de la captura. Mientras no podía representarse, un combo
// sin elegir se guardaba en silencio como el primer valor del enum, un dato
// inventado que nadie capturó. Por eso sus getters devuelven std::optional y
// validate() exige los tres. Los setters siguen devolviendo void: recibir un
// valor del enum no puede fallar; lo único que puede pasar es que no llegue
// ninguno.
class VehicleConditions
{
public:
    VehicleConditions() = default;

    const CatalogRef &fuelType() const;
    void setFuelType(const CatalogRef &value);

    int cylinders() const;
    [[nodiscard]] bool setCylinders(int value);

    std::optional<Transmission> transmission() const;
    void setTransmission(Transmission value);

    // Texto libre a propósito: la columna no tiene CHECK y el combo de la
    // interfaz es editable, porque los tapizados no son un conjunto cerrado.
    const QString &interiorMaterial() const;
    [[nodiscard]] bool setInteriorMaterial(const QString &value);

    std::optional<WindowRegulators> windowRegulators() const;
    void setWindowRegulators(WindowRegulators value);

    std::optional<AirConditioning> airConditioning() const;
    void setAirConditioning(AirConditioning value);

    ValidationResult validate() const;

private:
    CatalogRef m_fuelType;
    int m_cylinders = 0;
    std::optional<Transmission> m_transmission;
    QString m_interiorMaterial;
    std::optional<WindowRegulators> m_windowRegulators;
    std::optional<AirConditioning> m_airConditioning;
};

} // namespace domain

#endif // DOMAIN_MODEL_VEHICLECONDITIONS_H
