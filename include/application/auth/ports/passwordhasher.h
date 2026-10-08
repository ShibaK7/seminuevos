#ifndef APPLICATION_AUTH_PORTS_PASSWORDHASHER_H
#define APPLICATION_AUTH_PORTS_PASSWORDHASHER_H

#include <QString>

namespace application {

// Puerto: cómo se guardan y comprueban las contraseñas. La implementación real
// (Pbkdf2PasswordHasher) usa Qt Network y tarda a propósito (210,000
// iteraciones); las pruebas usan una instantánea. Los métodos son const y sin
// estado, así que se pueden llamar desde cualquier hilo.
class PasswordHasher
{
public:
    virtual ~PasswordHasher() = default;

    virtual QString hash(const QString &plainPassword) const = 0;
    virtual bool verify(const QString &plainPassword, const QString &storedHash) const = 0;

protected:
    PasswordHasher() = default;
    PasswordHasher(const PasswordHasher &) = default;
    PasswordHasher &operator=(const PasswordHasher &) = default;
};

} // namespace application

#endif // APPLICATION_AUTH_PORTS_PASSWORDHASHER_H
