#include "domain/model/counterparty.h"

#include <QRegularExpression>
#include <QStringList>

namespace domain {
namespace {

// Topes de los VARCHAR de la tabla counterparties. Rechazar aquí evita que
// PostgreSQL trunque o falle con un mensaje mucho menos claro.
constexpr int kFullNameMaxLength = 100;
constexpr int kNationalIdMaxLength = 50;
constexpr int kSuburbMaxLength = 100;
constexpr int kLocalityMaxLength = 100;
constexpr int kStateMaxLength = 50;
constexpr int kEmailMaxLength = 100;

constexpr int kPostalCodeDigits = 5;
constexpr int kPhoneDigits = 10;

QString digitsOnly(const QString &value)
{
    QString digits;
    digits.reserve(value.size());
    for (QChar c : value) {
        if (c.isDigit())
            digits.append(c);
    }
    return digits;
}

// Deliberadamente laxo: no intenta implementar RFC 5322, solo descartar lo
// que claramente no es una dirección. Un validador estricto rechaza correos
// válidos y raros, que es peor que dejar pasar uno malformado.
bool looksLikeEmail(const QString &value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]{2,}$"));
    return pattern.match(value).hasMatch();
}

} // namespace

int Counterparty::id() const
{
    return m_id;
}

bool Counterparty::isPersisted() const
{
    return m_id > 0;
}

void Counterparty::assignId(int id)
{
    m_id = id;
}

const QString &Counterparty::fullName() const
{
    return m_fullName;
}

bool Counterparty::setFullName(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.length() > kFullNameMaxLength)
        return false;
    m_fullName = trimmed;
    return true;
}

const QString &Counterparty::nationalId() const
{
    return m_nationalId;
}

bool Counterparty::setNationalId(const QString &value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized.length() > kNationalIdMaxLength)
        return false;
    m_nationalId = normalized;
    return true;
}

const QString &Counterparty::streetAddress() const
{
    return m_streetAddress;
}

void Counterparty::setStreetAddress(const QString &value)
{
    // Columna TEXT: sin tope que respetar, así que no puede fallar.
    m_streetAddress = value.trimmed();
}

const QString &Counterparty::suburb() const
{
    return m_suburb;
}

bool Counterparty::setSuburb(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.length() > kSuburbMaxLength)
        return false;
    m_suburb = trimmed;
    return true;
}

const QString &Counterparty::locality() const
{
    return m_locality;
}

bool Counterparty::setLocality(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.length() > kLocalityMaxLength)
        return false;
    m_locality = trimmed;
    return true;
}

const QString &Counterparty::state() const
{
    return m_state;
}

bool Counterparty::setState(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.length() > kStateMaxLength)
        return false;
    m_state = trimmed;
    return true;
}

const QString &Counterparty::postalCode() const
{
    return m_postalCode;
}

bool Counterparty::setPostalCode(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        m_postalCode = QString();
        return true;
    }
    if (trimmed.length() != kPostalCodeDigits || digitsOnly(trimmed).length() != kPostalCodeDigits)
        return false;
    m_postalCode = trimmed;
    return true;
}

const QString &Counterparty::phone() const
{
    return m_phone;
}

bool Counterparty::setPhone(const QString &value)
{
    // Vaciar el campo es una operación legítima; escribir algo que no es un
    // teléfono, no. Sin esta distinción, pasar "abc" borraría el número que
    // ya estaba guardado y devolvería éxito.
    if (value.trimmed().isEmpty()) {
        m_phone = QString();
        return true;
    }

    const QString digits = digitsOnly(value);
    if (digits.length() != kPhoneDigits)
        return false;
    m_phone = digits;
    return true;
}

const QString &Counterparty::email() const
{
    return m_email;
}

bool Counterparty::setEmail(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        m_email = QString();
        return true;
    }
    if (trimmed.length() > kEmailMaxLength || !looksLikeEmail(trimmed))
        return false;
    m_email = trimmed;
    return true;
}

QString Counterparty::formattedAddress() const
{
    QStringList parts;
    if (!m_streetAddress.isEmpty())
        parts << m_streetAddress;
    if (!m_suburb.isEmpty())
        parts << m_suburb;
    if (!m_locality.isEmpty())
        parts << m_locality;
    if (!m_state.isEmpty())
        parts << m_state;
    if (!m_postalCode.isEmpty())
        parts << QStringLiteral("C.P. %1").arg(m_postalCode);
    return parts.join(QStringLiteral(", "));
}

bool Counterparty::hasCompleteAddress() const
{
    return !m_streetAddress.isEmpty() && !m_suburb.isEmpty() && !m_locality.isEmpty()
           && !m_state.isEmpty() && !m_postalCode.isEmpty();
}

ValidationResult Counterparty::validate() const
{
    ValidationResult result;
    if (m_fullName.isEmpty()) {
        result.addError(QStringLiteral("fullName"),
                        QStringLiteral("Captura el nombre completo."));
    }
    if (m_nationalId.isEmpty()) {
        result.addError(QStringLiteral("nationalId"),
                        QStringLiteral("Captura la identificación."));
    }
    return result;
}

} // namespace domain
