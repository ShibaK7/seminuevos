#ifndef ADAPTERS_PERSISTENCE_SQLINVENTORYREADER_H
#define ADAPTERS_PERSISTENCE_SQLINVENTORYREADER_H

#include "application/ports/inventoryreader.h"

class ConnectionPool;

// Adaptador del puerto InventoryReader sobre PostgreSQL: la proyección de
// lectura de la rejilla, sin transacción ni objetos de dominio.
//
// Trae exactamente lo que pinta una tarjeta. Una unidad completa arrastra su
// inspección (hasta 37 renglones por unidad) y la tarjeta no muestra ninguno:
// para cien vehículos serían casi cuatro mil filas para pintar cien
// rectángulos.
class SqlInventoryReader final : public application::InventoryReader
{
public:
    explicit SqlInventoryReader(ConnectionPool &pool);

    QList<application::InventoryItemDto> search(const application::InventoryFilterDto &filter,
                                                QString *error) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_SQLINVENTORYREADER_H
