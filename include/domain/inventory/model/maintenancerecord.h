#ifndef DOMAIN_INVENTORY_MODEL_MAINTENANCERECORD_H
#define DOMAIN_INVENTORY_MODEL_MAINTENANCERECORD_H

#include "domain/common/value_objects/validationresult.h"

#include <QDate>
#include <QString>

namespace domain {

// Un mantenimiento hecho a una unidad. Mapea un renglón de vehicle_maintenance
// (US-03.3, #46).
//
// El costo llega como el texto que se capturó y es esta clase la que decide si
// es un número que cabe en la columna: así la regla vive en el dominio, y la
// vista no convierte ni redondea nada antes de que se valide.
class MaintenanceRecord
{
public:
    // cost NUMERIC(12, 2): doce dígitos, dos de ellos decimales.
    static constexpr int kCostIntegerDigits = 10;
    static constexpr int kCostDecimals = 2;

    // typeId es el id de vehicle_maintenance_cat; -1 si no se eligió.
    MaintenanceRecord(int vehicleFolio, const QDate &date, int typeId, const QString &cost,
                      const QString &description);

    int vehicleFolio() const;
    const QDate &date() const;
    int typeId() const;
    // El costo tal como se guarda ("1500.50"), sin espacios alrededor. Solo
    // es un número válido si validate() no reporta error en "cost".
    const QString &cost() const;
    // Sin espacios alrededor; solo de espacios cuenta como vacía.
    const QString &description() const;

    // Campos: "maintenanceDate", "maintenanceType", "cost", "description" y
    // "folio" (este último no se captura: falla solo si la unidad no está
    // guardada).
    ValidationResult validate() const;

    // La regla de la descripción sola, para corregirla en un mantenimiento ya
    // guardado sin rearmar el renglón completo. Campo "description".
    static ValidationResult validateDescription(const QString &description);

private:
    int m_vehicleFolio;
    QDate m_date;
    int m_typeId;
    QString m_cost;
    QString m_description;
};

} // namespace domain

#endif // DOMAIN_INVENTORY_MODEL_MAINTENANCERECORD_H
