#ifndef APPLICATION_PORTS_REFERENCEDATAREADER_H
#define APPLICATION_PORTS_REFERENCEDATAREADER_H

#include "application/dto/catalogdtos.h"

#include <QList>
#include <QString>

#include <optional>

namespace application {

// Puerto: catálogos y parámetros de solo lectura. Uno solo para todos los
// catálogos, con un método por catálogo: crear un repositorio por combo sería
// ceremonia sin beneficio. Reemplaza al SQL que las vistas corrían directo
// (UIUtils, ConditionCatalog y la lectura de la UMA).
//
// En todos los métodos, si la consulta falla devuelve vacío y deja la causa en
// `error`.
class ReferenceDataReader
{
public:
    virtual ~ReferenceDataReader() = default;

    // Todas las categorías: las raíz con parentId -1 y los subtipos con el id
    // de su tipo.
    virtual QList<CatalogOptionDto> vehicleCategories(QString *error) = 0;
    virtual QList<CatalogOptionDto> brands(QString *error) = 0;
    virtual QList<CatalogOptionDto> fuelTypes(QString *error) = 0;
    // Ordenado y con las categorías contiguas.
    virtual QList<ChecklistItemDto> conditionChecklist(QString *error) = 0;
    // nullopt si no está configurada.
    virtual std::optional<double> umaDailyValue(QString *error) = 0;

protected:
    ReferenceDataReader() = default;
    ReferenceDataReader(const ReferenceDataReader &) = default;
    ReferenceDataReader &operator=(const ReferenceDataReader &) = default;
};

} // namespace application

#endif // APPLICATION_PORTS_REFERENCEDATAREADER_H
