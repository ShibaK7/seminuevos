#ifndef APPLICATION_INVENTORY_SERVICES_INVENTORYSERVICE_H
#define APPLICATION_INVENTORY_SERVICES_INVENTORYSERVICE_H

#include "application/inventory/dto/inventorydtos.h"
#include "application/common/ports/filestorage.h"
#include "application/inventory/ports/inventoryreader.h"

namespace application {

// Caso de uso "consultar el inventario". Antes la ventana principal corría el
// SQL y cargaba las fotos del disco en el hilo de la interfaz.
//
// Solo guarda referencias a sus puertos: se llama desde el hilo de trabajo.
class InventoryService final
{
public:
    InventoryService(InventoryReader &reader, FileStorage &files);

    // Las unidades que cumplen el filtro, con los bytes de su portada. Hace
    // E/S: correrlo fuera del hilo de la interfaz.
    InventoryResultDto search(const InventoryFilterDto &filter) const;

private:
    InventoryReader &m_reader;
    FileStorage &m_files;
};

} // namespace application

#endif // APPLICATION_INVENTORY_SERVICES_INVENTORYSERVICE_H
