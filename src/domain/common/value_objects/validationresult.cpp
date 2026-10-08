#include "domain/common/value_objects/validationresult.h"

namespace domain {

void ValidationResult::addError(QString field, QString message)
{
    m_errors.append(ValidationError{std::move(field), std::move(message)});
}

void ValidationResult::merge(const ValidationResult &other, const QString &fieldPrefix)
{
    for (const ValidationError &error : other.m_errors) {
        if (fieldPrefix.isEmpty()) {
            m_errors.append(error);
        } else {
            m_errors.append(ValidationError{fieldPrefix + QLatin1Char('.') + error.field,
                                            error.message});
        }
    }
}

bool ValidationResult::isValid() const
{
    return m_errors.isEmpty();
}

const QList<ValidationError> &ValidationResult::errors() const
{
    return m_errors;
}

QString ValidationResult::firstMessage() const
{
    return m_errors.isEmpty() ? QString() : m_errors.first().message;
}

QString ValidationResult::joinedMessages(const QString &separator) const
{
    QStringList messages;
    messages.reserve(m_errors.size());
    for (const ValidationError &error : m_errors)
        messages << error.message;
    return messages.join(separator);
}

QStringList ValidationResult::fields() const
{
    QStringList result;
    result.reserve(m_errors.size());
    for (const ValidationError &error : m_errors)
        result << error.field;
    return result;
}

} // namespace domain
