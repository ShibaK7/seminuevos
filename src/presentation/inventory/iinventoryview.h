#ifndef PRESENTATION_INVENTORY_IINVENTORYVIEW_H
#define PRESENTATION_INVENTORY_IINVENTORYVIEW_H

#include "application/inventory/dto/inventorydtos.h"

#include <QList>
#include <QString>

namespace presentation {

// La rejilla del inventario con su barra de filtros.
class IInventoryView
{
public:
    virtual ~IInventoryView() = default;

    // Lo que está elegido ahora en la barra de filtros.
    virtual application::InventoryFilterDto filter() const = 0;

    virtual void showVehicles(const QList<application::InventoryItemDto> &vehicles) = 0;
    // Un aviso en lugar de las tarjetas (error o "no hay vehículos").
    virtual void showInventoryMessage(const QString &message) = 0;
    virtual void scrollToFolio(int folio) = 0;

protected:
    IInventoryView() = default;
    IInventoryView(const IInventoryView &) = default;
    IInventoryView &operator=(const IInventoryView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_INVENTORY_IINVENTORYVIEW_H
