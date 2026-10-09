#ifndef ADAPTERS_SECURITY_PBKDF2PASSWORDHASHER_H
#define ADAPTERS_SECURITY_PBKDF2PASSWORDHASHER_H

#include "application/auth/ports/passwordhasher.h"

#include <QString>

// Adaptador del puerto PasswordHasher: PBKDF2-HMAC-SHA256 sin dependencias
// externas (QPasswordDigestor, del módulo Qt Network, implementa PBKDF2 según
// RFC 8018). El formato guardado es autodescriptivo, para poder cambiar
// iteraciones o algoritmo a futuro sin romper hashes ya guardados:
// "pbkdf2-sha256$<iteraciones>$<salt_base64>$<hash_base64>".
//
// Antes era un namespace con funciones libres; ahora es una clase que
// implementa el puerto, para que el caso de uso de inicio de sesión no dependa
// de Qt Network y las pruebas puedan sustituirlo por uno instantáneo.
class Pbkdf2PasswordHasher final : public application::PasswordHasher
{
public:
    QString hash(const QString &plainPassword) const override;
    bool verify(const QString &plainPassword, const QString &storedHash) const override;
};

#endif // ADAPTERS_SECURITY_PBKDF2PASSWORDHASHER_H
