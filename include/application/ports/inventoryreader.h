#ifndef APPLICATION_PORTS_INVENTORYREADER_H
#define APPLICATION_PORTS_INVENTORYREADER_H

#include "application/dto/inventorydtos.h"

#include <QList>
#include <QString>

namespace application {

// Puerto: la lectura del inventario para la rejilla de tarjetas. Va aparte de
// VehicleRepository a propósito: aquél es de escritura (una transacción, seis
// tablas) y esto es una proyección de lectura sin objetos de dominio.
class InventoryReader
{
public:
    virtual ~InventoryReader() = default;

    // Las unidades que cumplen el filtro, la más reciente primero. Si falla,
    // devuelve una lista vacía y llena `error`. coverImage viene vacío: lo
    // llena el servicio.
    virtual QList<InventoryItemDto> search(const InventoryFilterDto &filter, QString *error) = 0;

protected:
    InventoryReader() = default;
    InventoryReader(const InventoryReader &) = default;
    InventoryReader &operator=(const InventoryReader &) = default;
};

} // namespace application

#endif // APPLICATION_PORTS_INVENTORYREADER_H
