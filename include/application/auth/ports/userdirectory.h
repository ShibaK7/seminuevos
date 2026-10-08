#ifndef APPLICATION_AUTH_PORTS_USERDIRECTORY_H
#define APPLICATION_AUTH_PORTS_USERDIRECTORY_H

#include "application/auth/dto/authdtos.h"

#include <QString>

#include <optional>

namespace application {

// Puerto: de dónde salen las cuentas de usuario. La aplicación no sabe que
// detrás hay una tabla de PostgreSQL; el adaptador (SqlUserDirectory) sí.
// En las pruebas lo implementa un directorio en memoria.
class UserDirectory
{
public:
    virtual ~UserDirectory() = default;

    // nullopt si no existe la cuenta. Si falla la consulta, también nullopt
    // y además `error` (cuando no es nulo) recibe la causa, para distinguir
    // "no existe" de "no se pudo preguntar".
    virtual std::optional<UserRecordDto> findByUsername(const QString &username,
                                                        QString *error) = 0;

protected:
    UserDirectory() = default;
    UserDirectory(const UserDirectory &) = default;
    UserDirectory &operator=(const UserDirectory &) = default;
};

} // namespace application

#endif // APPLICATION_AUTH_PORTS_USERDIRECTORY_H
