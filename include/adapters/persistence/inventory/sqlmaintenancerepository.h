#ifndef ADAPTERS_PERSISTENCE_INVENTORY_SQLMAINTENANCEREPOSITORY_H
#define ADAPTERS_PERSISTENCE_INVENTORY_SQLMAINTENANCEREPOSITORY_H

#include "application/inventory/maintenance/ports/maintenancerepository.h"

#include <QList>
#include <QVariant>

#include <utility>

class ConnectionPool;

// Adaptador del puerto MaintenanceRepository sobre vehicle_maintenance.
//
// Al agregar, el costo se suma al maintenance_cost de la subtabla de la
// operación (vehicle_acquisitions o vehicle_consignments) en la misma
// transacción. Se suma, no se recalcula como suma del historial: borraría el
// costo capturado en el alta. Y no toca vehicles: un administrador que esté
// editando esa unidad recibiría un conflicto de versión al guardar.
class SqlMaintenanceRepository final : public application::MaintenanceRepository
{
public:
    explicit SqlMaintenanceRepository(ConnectionPool &pool);

    QList<application::MaintenanceItemDto> listForVehicle(int folio, QString *error) override;
    AddOutcome add(const domain::MaintenanceRecord &record) override;
    bool updateDescription(int id, const QString &description, QString *error) override;
    bool markReceiptPrinted(int id, QString *error) override;

private:
    // Un UPDATE que debe tocar exactamente un renglón de vehicle_maintenance.
    bool updateOne(const QString &sql, const QList<std::pair<QString, QVariant>> &values,
                   QString *error);

    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_INVENTORY_SQLMAINTENANCEREPOSITORY_H
