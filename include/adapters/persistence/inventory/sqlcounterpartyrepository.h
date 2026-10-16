#ifndef ADAPTERS_PERSISTENCE_INVENTORY_SQLCOUNTERPARTYREPOSITORY_H
#define ADAPTERS_PERSISTENCE_INVENTORY_SQLCOUNTERPARTYREPOSITORY_H

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

    struct Relink
    {
        // Con qué contraparte queda la operación; -1 si no se pudo.
        int id = -1;
        // La persona cambió desde que se leyó (otro usuario la corrigió).
        bool conflict = false;
        // La identificación ya es de otra persona, con otro nombre: el suyo.
        QString takenBy;
        QString errorMessage;
    };

    // Al corregir una operación cuya contraparte el usuario cambió: con cuál
    // queda, partiendo de la que tenía (`currentId`) y de lo capturado.
    //   - Misma identificación: es la misma persona, y sus datos se corrigen
    //     ahí, así que la corrección se ve en todas sus operaciones. Solo si
    //     su renglón sigue en la versión que se leyó (`expectedVersion`); si
    //     no, es un conflicto: no se pisa lo que otro usuario corrigió. La
    //     identificación, el teléfono y el correo no se tocan.
    //   - Otra identificación: la operación pasa a la persona que la tiene,
    //     sin modificarla, o a una nueva. Si esa persona tiene otro nombre no
    //     se liga: se devuelve su nombre en takenBy, para que el usuario lo
    //     revise en vez de cambiar en silencio lo que ve.
    Relink keepOrRelink(int currentId, const domain::Counterparty &edited,
                        const QString &expectedVersion);

private:
    int insert(const domain::Counterparty &counterparty, QString &errorMessage);

    QSqlDatabase &m_db;
};

#endif // ADAPTERS_PERSISTENCE_INVENTORY_SQLCOUNTERPARTYREPOSITORY_H
