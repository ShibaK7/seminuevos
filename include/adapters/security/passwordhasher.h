#ifndef ADAPTERS_SECURITY_PASSWORDHASHER_H
#define ADAPTERS_SECURITY_PASSWORDHASHER_H

#include <QString>

// Hasheo de contraseñas con PBKDF2-HMAC-SHA256 (sin dependencias externas:
// QPasswordDigestor, del módulo Qt Network, implementa PBKDF2 según RFC
// 8018). El formato guardado es autodescriptivo, para poder cambiar
// iteraciones/algoritmo a futuro sin romper hashes ya guardados:
// "pbkdf2-sha256$<iteraciones>$<salt_base64>$<hash_base64>".
namespace PasswordHasher
{
QString hash(const QString &plainPassword);
bool verify(const QString &plainPassword, const QString &storedHash);
} // namespace PasswordHasher

#endif // ADAPTERS_SECURITY_PASSWORDHASHER_H
