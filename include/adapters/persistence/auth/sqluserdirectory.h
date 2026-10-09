#ifndef ADAPTERS_PERSISTENCE_AUTH_SQLUSERDIRECTORY_H
#define ADAPTERS_PERSISTENCE_AUTH_SQLUSERDIRECTORY_H

#include "application/auth/ports/userdirectory.h"

class ConnectionPool;

// Adaptador del puerto UserDirectory sobre la tabla users de PostgreSQL. Es el
// SQL que antes vivía dentro de LoginWorker.
//
// Pide la conexión en cada llamada, no la guarda: así sirve desde cualquier
// hilo (QSqlDatabase solo puede usarse en el hilo que la creó, y el pool le da
// a cada hilo la suya).
class SqlUserDirectory final : public application::UserDirectory
{
public:
    explicit SqlUserDirectory(ConnectionPool &pool);

    std::optional<application::UserRecordDto> findByUsername(const QString &username,
                                                             QString *error) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_AUTH_SQLUSERDIRECTORY_H
