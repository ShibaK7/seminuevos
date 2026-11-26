#ifndef ADAPTERS_PERSISTENCE_SETTINGS_SQLSETTINGSREPOSITORY_H
#define ADAPTERS_PERSISTENCE_SETTINGS_SQLSETTINGSREPOSITORY_H

#include "application/settings/ports/settingsrepository.h"

class ConnectionPool;

// Adaptador del puerto SettingsRepository sobre global_configurations
// (US-07.1, #83 y #84).
//
// Al guardar, cada parámetro que cambió se escribe con su valor esperado en el
// WHERE: si otro administrador lo cambió mientras tanto, no se pisa, y la
// transacción completa se deshace. Solo los que cambian reciben fecha nueva.
class SqlSettingsRepository final : public application::SettingsRepository
{
public:
    explicit SqlSettingsRepository(ConnectionPool &pool);

    QList<application::SystemParameterDto> getSystemParameters(QString *error) override;
    UpdateOutcome updateSystemParameters(const QList<Change> &changes) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_SETTINGS_SQLSETTINGSREPOSITORY_H
