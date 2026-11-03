#ifndef ADAPTERS_PERSISTENCE_COMMON_SQLCOUNTERPARTYDIRECTORY_H
#define ADAPTERS_PERSISTENCE_COMMON_SQLCOUNTERPARTYDIRECTORY_H

#include "application/common/ports/counterpartydirectory.h"

class ConnectionPool;

// Pool-backed lookup/update adapter for the shared counterparties table.
class SqlCounterpartyDirectory final : public application::CounterpartyDirectory
{
public:
    explicit SqlCounterpartyDirectory(ConnectionPool &pool);

    QList<application::CounterpartyDto> search(const QString &query, QString *error) override;
    application::CounterpartyUpdateResultDto update(
        const application::CounterpartyDto &counterparty) override;

private:
    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_COMMON_SQLCOUNTERPARTYDIRECTORY_H