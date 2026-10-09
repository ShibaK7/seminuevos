#include "adapters/security/pbkdf2passwordhasher.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>

namespace {

constexpr int kIterations = 210000;
constexpr int kSaltBytes = 16;
constexpr int kKeyBytes = 32;
const QString kAlgoTag = QStringLiteral("pbkdf2-sha256");

QByteArray randomSalt()
{
    QByteArray salt(kSaltBytes, char(0));
    auto *data = reinterpret_cast<quint32 *>(salt.data());
    QRandomGenerator::global()->fillRange(data, kSaltBytes / sizeof(quint32));
    return salt;
}

// Comparación en tiempo constante para no filtrar por temporización cuánto
// del hash coincide.
bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

} // namespace

QString Pbkdf2PasswordHasher::hash(const QString &plainPassword) const
{
    const QByteArray salt = randomSalt();
    const QByteArray derived = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, plainPassword.toUtf8(), salt, kIterations, kKeyBytes);

    return QStringLiteral("%1$%2$%3$%4")
        .arg(kAlgoTag)
        .arg(kIterations)
        .arg(QString::fromLatin1(salt.toBase64()))
        .arg(QString::fromLatin1(derived.toBase64()));
}

bool Pbkdf2PasswordHasher::verify(const QString &plainPassword, const QString &storedHash) const
{
    const QStringList parts = storedHash.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts[0] != kAlgoTag)
        return false;

    bool iterationsOk = false;
    const int iterations = parts[1].toInt(&iterationsOk);
    if (!iterationsOk || iterations <= 0)
        return false;

    const QByteArray salt = QByteArray::fromBase64(parts[2].toLatin1());
    const QByteArray expected = QByteArray::fromBase64(parts[3].toLatin1());

    const QByteArray actual = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, plainPassword.toUtf8(), salt, iterations, expected.size());

    return constantTimeEquals(actual, expected);
}
