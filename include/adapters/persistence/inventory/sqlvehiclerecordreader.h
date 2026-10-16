#ifndef ADAPTERS_PERSISTENCE_INVENTORY_SQLVEHICLERECORDREADER_H
#define ADAPTERS_PERSISTENCE_INVENTORY_SQLVEHICLERECORDREADER_H

#include "application/inventory/registration/ports/vehiclerecordreader.h"

class ConnectionPool;
class QSqlDatabase;

// Adaptador del puerto VehicleRecordReader sobre PostgreSQL (#30): el
// expediente completo de un folio -- la unidad con los nombres de sus
// catálogos, la operación de su rama con la contraparte, las
// especificaciones, la inspección y las rutas de fotos y documentos.
//
// Son cuatro consultas (la unidad con todo lo que es uno a uno, y la
// inspección, las fotos y los documentos, que son listas) dentro de una
// transacción de solo lectura en REPEATABLE READ: las cuatro ven la misma foto
// de la base, así que la versión que se lee corresponde a los hijos que se
// leen aunque otro usuario guarde a la mitad.
//
// Pide la conexión en cada llamada (no la guarda), así que sirve desde el hilo
// de trabajo.
class SqlVehicleRecordReader final : public application::VehicleRecordReader
{
public:
    explicit SqlVehicleRecordReader(ConnectionPool &pool);

    std::optional<application::VehicleRecordDto> findByFolio(int folio, QString *error) override;

private:
    // Lee dentro de la transacción ya abierta. nullopt con `error` vacío = no
    // existe.
    static std::optional<application::VehicleRecordDto> read(QSqlDatabase &db, int folio,
                                                             QString &error);

    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_INVENTORY_SQLVEHICLERECORDREADER_H
