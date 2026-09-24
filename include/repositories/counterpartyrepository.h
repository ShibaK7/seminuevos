#ifndef COUNTERPARTYREPOSITORY_H
#define COUNTERPARTYREPOSITORY_H

#include <QString>
#include <QList>

class CounterpartyRepository {
public:
    CounterpartyRepository() = delete;

    static QList<CounterpartyLookupDTO> searchByName(const QString& nameQuery, int limit = 10);

    static bool getCounterpartyById(int id, CounterpartyDTO& outRecord);

    static bool addCounterparty(const CounterpartyDTO& record, int& outId);
};

#endif // COUNTERPARTYREPOSITORY_H