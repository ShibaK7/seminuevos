#ifndef SCHEMAINITIALIZER_H
#define SCHEMAINITIALIZER_H

#include <QString>

class QSqlDatabase;

// Aplica db/001_init_schema.sql (empaquetado como recurso Qt en
// :/db/001_init_schema.sql) contra la base de datos.
//
// El script NO es idempotente: empieza con DROP TABLE ... CASCADE. Por eso
// no se ejecuta en cada arranque, sino solo cuando la versión instalada en
// la tabla schema_version no coincide con kSchemaVersion. En uso normal la
// app arranca, ve que la versión coincide y no toca nada.
namespace SchemaInitializer
{
// Súbela en 1 cada vez que cambies db/001_init_schema.sql. El siguiente
// arranque detecta el desfase y vuelve a correr el script COMPLETO.
//
//     *** SUBIR ESTE NÚMERO BORRA TODOS LOS DATOS DE LA BASE. ***
//
// Es el reset controlado de la fase de definición: mientras el esquema
// cambie seguido sale más barato regenerarlo que escribir migraciones. En
// cuanto haya datos reales de la concesionaria, esto se reemplaza por
// migraciones incrementales (002_*.sql, 003_*.sql...) y este mecanismo se
// retira.
inline constexpr int kSchemaVersion = 2;

struct Result
{
    bool ok = false;
    // true si se ejecutó el script, es decir, si hubo reset destructivo.
    bool applied = false;
    // Versión que había instalada antes de correr. -1 = base virgen o
    // anterior al versionado.
    int previousVersion = -1;
    // true si al arrancar ya existían tablas del esquema, es decir, si el
    // reset destruyó algo. Hace falta aparte de previousVersion porque una
    // base creada antes de que existiera el versionado reporta -1 y sin
    // embargo puede estar llena de datos: mirar solo la versión haría que
    // justo esa primera migración borrara todo en silencio.
    bool hadExistingSchema = false;
    QString errorMessage;
};

// allowDestructiveReset = false hace que run() se niegue a borrar y devuelva
// un error explicativo cuando detecta desfase de versión. Es la válvula de
// seguridad para ambientes con datos reales: sin ella, el día que salga una
// build con kSchemaVersion subida arrancaría contra la base de la
// concesionaria y la borraría sin preguntar.
Result run(QSqlDatabase &db, bool allowDestructiveReset = true);
} // namespace SchemaInitializer

#endif // SCHEMAINITIALIZER_H
