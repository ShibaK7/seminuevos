#ifndef APPLICATION_AUTH_DTO_AUTHDTOS_H
#define APPLICATION_AUTH_DTO_AUTHDTOS_H

#include <QString>

namespace application {

// Lo que el directorio de usuarios devuelve de una cuenta. Solo lo que el
// caso de uso "iniciar sesión" necesita: no es una copia de la tabla users.
struct UserRecordDto
{
    QString passwordHash;
    QString displayName;
    QString role;
};

// La sesión que obtiene quien inició sesión. Es lo único que cruza hacia la
// interfaz: nunca el hash.
struct SessionDto
{
    QString username;
    QString displayName;
    QString role;
};

struct LoginResult
{
    bool ok = false;
    SessionDto session;
    QString errorMessage;
};

} // namespace application

#endif // APPLICATION_AUTH_DTO_AUTHDTOS_H
