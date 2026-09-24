#ifndef COUNTERPARTYSERVICE_H
#define COUNTERPARTYSERVICE_H

#include "counterpartyrepository.h"
#include <QString>
#include <QList>

class CounterpartyService {
public:
    CounterpartyService() = delete;

    static QList<CounterpartyLookupDTO> searchByName(const QString& query, int limit = 10);

    static bool getCounterpartyById(int id, CounterpartyDTO& outDto, QString& errorMessage);

    static bool saveCounterparty(CounterpartyDTO& dto, int& outId, QString& errorMessage);

    static bool validate(const CounterpartyDTO& dto, QString& errorMessage);
};

#endif // COUNTERPARTYSERVICE_H