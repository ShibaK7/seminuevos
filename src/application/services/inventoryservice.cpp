#include "application/services/inventoryservice.h"

namespace application {

namespace {

// Tamaño máximo de la miniatura de portada: el doble de lo que mide la foto en
// la tarjeta, para que se vea nítida en pantallas con escala 2x.
constexpr int kCoverWidth = 480;
constexpr int kCoverHeight = 360;

} // namespace

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

    // La portada llega como miniatura, generada aquí, fuera del hilo de la
    // interfaz: decodificar cientos de fotos de 1280x960 en ese hilo congelaba
    // la rejilla. Una foto que ya no está en disco (o una raíz de
    // almacenamiento distinta de la que la escribió) queda vacía y la tarjeta
    // pone el marcador de posición.
    for (InventoryItemDto &item : result.vehicles) {
        if (!item.coverImagePath.isEmpty()) {
            item.coverImage = m_files.readThumbnail(m_files.absolutePath(item.coverImagePath),
                                                    kCoverWidth, kCoverHeight);
        }
    }
    return result;
}

} // namespace application
