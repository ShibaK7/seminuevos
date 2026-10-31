#ifndef APPLICATION_COMMON_PORTS_COUNTERPARTYDIRECTORY_H
#define APPLICATION_COMMON_PORTS_COUNTERPARTYDIRECTORY_H

#include "application/common/dto/counterpartydtos.h"

namespace application {

// Shared lookup/update boundary for people who can take different roles in
// different operations. Search and update are separate so callers can require
// an explicit user choice before changing a shared record.
class CounterpartyDirectory
{
public:
    virtual ~CounterpartyDirectory() = default;

    virtual QList<CounterpartyDto> search(const QString &query, QString *error) = 0;
    virtual CounterpartyUpdateResultDto update(const CounterpartyDto &counterparty) = 0;

protected:
    CounterpartyDirectory() = default;
    CounterpartyDirectory(const CounterpartyDirectory &) = default;
    CounterpartyDirectory &operator=(const CounterpartyDirectory &) = default;
};

} // namespace application

#endif // APPLICATION_COMMON_PORTS_COUNTERPARTYDIRECTORY_H
