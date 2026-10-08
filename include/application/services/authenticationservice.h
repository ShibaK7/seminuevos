#ifndef APPLICATION_SERVICES_AUTHENTICATIONSERVICE_H
#define APPLICATION_SERVICES_AUTHENTICATIONSERVICE_H

#include "application/dto/authdtos.h"
#include "application/ports/passwordhasher.h"
#include "application/ports/userdirectory.h"

namespace application {

// Caso de uso "iniciar sesión". Antes vivía repartido entre LoginWorker (un
// QThread con el SQL y la verificación adentro) y lambdas de main.cpp.
//
// No guarda estado más allá de sus puertos, así que se puede llamar desde el
// hilo de trabajo. Cuál hilo es asunto de la presentación (TaskRunner), no
// de este servicio.
class AuthenticationService final
{
public:
    AuthenticationService(UserDirectory &users, const PasswordHasher &hasher);

    LoginResult login(const QString &username, const QString &password) const;

private:
    UserDirectory &m_users;
    const PasswordHasher &m_hasher;
};

} // namespace application

#endif // APPLICATION_SERVICES_AUTHENTICATIONSERVICE_H
