#include "application/services/inventoryservice.h"

namespace application {

InventoryService::InventoryService(InventoryReader &reader, FileStorage &files)
    : m_reader(reader)
    , m_files(files)
{
}

InventoryResultDto InventoryService::search(const InventoryFilterDto &filter) const
{
    InventoryResultDto result;
    result.vehicles = m_reader.search(filter, &result.errorMessage);
    if (!result.errorMessage.isEmpty()) {
        result.vehicles.clear();
        return result;
    }

    // La portada se lee aquí, fuera del hilo de la interfaz. Una foto que ya
    // no está en disco (o una raíz de almacenamiento distinta de la que la
    // escribió) queda vacía y la tarjeta pone el marcador de posición.
    for (InventoryItemDto &item : result.vehicles) {
        if (!item.coverImagePath.isEmpty())
            item.coverImage = m_files.read(m_files.absolutePath(item.coverImagePath));
    }
    return result;
}

} // namespace application
