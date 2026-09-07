#ifndef DOMAIN_VEHICLECONDITIONS_H
#define DOMAIN_VEHICLECONDITIONS_H

#include "domain/catalogref.h"
#include "domain/enums.h"
#include "domain/validationresult.h"

#include <QString>

namespace domain {

// Especificaciones técnicas de la unidad: el panel izquierdo del Paso 2 del
// wizard. Mapea vehicle_conditions uno a uno.
//
// Va separada de Inspection a propósito, aunque las dos salgan de la misma
// pantalla: son tablas distintas y, sobre todo, se validan en momentos
// distintos. Tenerlas aparte permite validar el Paso 1 sin que salten
// errores de "faltan los cilindros", sin necesidad de banderas de alcance.
//
// Nótese que los setters de enum devuelven void: no pueden fallar. Es la
// prueba de que cambiar esos campos de QString a enum eliminó una categoría
// entera de validación en lugar de moverla de sitio.
class VehicleConditions
{
public:
    VehicleConditions() = default;

    const CatalogRef &fuelType() const;
    void setFuelType(const CatalogRef &value);

    int cylinders() const;
    [[nodiscard]] bool setCylinders(int value);

    Transmission transmission() const;
    void setTransmission(Transmission value);

    // Texto libre a propósito: la columna no tiene CHECK y el combo de la
    // interfaz es editable, porque los tapizados no son un conjunto cerrado.
    const QString &interiorMaterial() const;
    [[nodiscard]] bool setInteriorMaterial(const QString &value);

    WindowRegulators windowRegulators() const;
    void setWindowRegulators(WindowRegulators value);

    AirConditioning airConditioning() const;
    void setAirConditioning(AirConditioning value);

    ValidationResult validate() const;

private:
    CatalogRef m_fuelType;
    int m_cylinders = 0;
    Transmission m_transmission = Transmission::Manual;
    QString m_interiorMaterial;
    WindowRegulators m_windowRegulators = WindowRegulators::Manuales;
    AirConditioning m_airConditioning = AirConditioning::Manual;
};

} // namespace domain

#endif // DOMAIN_VEHICLECONDITIONS_H
