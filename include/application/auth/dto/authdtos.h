#ifndef APPLICATION_AUTH_DTO_AUTHDTOS_H
#define APPLICATION_AUTH_DTO_AUTHDTOS_H

#include "domain/common/value_objects/enums.h"

#include <QString>

namespace application {

// Lo que el directorio de usuarios devuelve de una cuenta. Solo lo que el
// caso de uso "iniciar sesión" necesita: no es una copia de la tabla users.
// El rol llega como el texto de la base; lo traduce el caso de uso.
struct UserRecordDto
{
    QString passwordHash;
    QString displayName;
    QString role;
    int id = -1;
};

// La sesión que obtiene quien inició sesión. Es lo único que cruza hacia la
// interfaz: nunca el hash. El rol ya viene traducido, así nadie compara
// textos ("Administrador", "administrador"...) para decidir qué se puede
// hacer. Por omisión, el de menos privilegios.
struct SessionDto
{
    QString username;
    QString displayName;
<<<<<<< HEAD
    QString role;
    int userId = -1;
=======
    domain::UserRole role = domain::UserRole::SalesAgent;
>>>>>>> 1041035 (Carry the session role as a domain enum)
};

struct LoginResult
{
    bool ok = false;
    SessionDto session;
    QString errorMessage;
};

} // namespace application

#endif // APPLICATION_AUTH_DTO_AUTHDTOS_H
