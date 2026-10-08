#ifndef ADAPTERS_PERSISTENCE_SQLCOUNTERPARTYREPOSITORY_H
#define ADAPTERS_PERSISTENCE_SQLCOUNTERPARTYREPOSITORY_H

#include <QString>

class QSqlDatabase;

namespace domain {
class Counterparty;
}

// Acceso a la tabla counterparties.
//
// Es un repositorio y no un método save() dentro de Counterparty a propósito:
// así el dominio no incluye Qt SQL y se puede razonar (y probar) sin levantar
// PostgreSQL. Además las conexiones de este proyecto son por hilo, y meter
// esa suposición dentro de un objeto que se copia libremente sería una
// trampa esperando a alguien.
class SqlCounterpartyRepository
{
public:
    explicit SqlCounterpartyRepository(QSqlDatabase &db);

    // Devuelve el id de la contraparte, creándola si no existía. La busca por
    // national_id; si viene vacío no hay con qué identificarla, así que se
    // inserta una nueva.
    //
    // Devuelve -1 en caso de error, con el detalle en errorMessage.
    int findOrCreate(const domain::Counterparty &counterparty, QString &errorMessage);

private:
    QSqlDatabase &m_db;
};

#endif // ADAPTERS_PERSISTENCE_SQLCOUNTERPARTYREPOSITORY_H
