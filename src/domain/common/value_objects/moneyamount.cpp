#include "domain/common/value_objects/moneyamount.h"

#include <QLatin1Char>

namespace domain {

namespace {

bool allDigits(const QString &text)
{
    for (const QChar c : text) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
    }
    return true;
}

} // namespace

MoneyAmount::Problem MoneyAmount::check(const QString &text)
{
    const QString amount = text.trimmed();
    if (amount.isEmpty())
        return Problem::Empty;
    if (amount.startsWith(QLatin1Char('-')))
        return Problem::Negative;

    const qsizetype point = amount.indexOf(QLatin1Char('.'));
    const QString integerPart = point < 0 ? amount : amount.left(point);
    const QString decimalPart = point < 0 ? QString() : amount.mid(point + 1);
    if (integerPart.isEmpty() || !allDigits(integerPart) || !allDigits(decimalPart)
        || (point >= 0 && decimalPart.isEmpty()))
        return Problem::NotANumber;
    if (decimalPart.size() > kDecimals)
        return Problem::TooManyDecimals;

    // Los ceros a la izquierda no cuentan contra el tope: "000123" es 123.
    qsizetype significant = 0;
    while (significant < integerPart.size() - 1 && integerPart.at(significant) == QLatin1Char('0'))
        ++significant;
    if (integerPart.size() - significant > kIntegerDigits)
        return Problem::TooManyIntegerDigits;
    return Problem::None;
}

std::optional<qint64> MoneyAmount::toCents(const QString &text)
{
    if (check(text) != Problem::None)
        return std::nullopt;
    const QString amount = text.trimmed();
    const qsizetype point = amount.indexOf(QLatin1Char('.'));
    const QString integerPart = point < 0 ? amount : amount.left(point);
    // Completa a dos decimales: "1500.5" son 50 centavos, no 5.
    const QString decimalPart =
        (point < 0 ? QString() : amount.mid(point + 1)).leftJustified(kDecimals, QLatin1Char('0'));
    // check() ya acotó los dígitos: caben en qint64 sin desbordar.
    return integerPart.toLongLong() * 100 + decimalPart.toLongLong();
}

QString MoneyAmount::fromCents(qint64 cents)
{
    return QStringLiteral("%1.%2").arg(cents / 100).arg(cents % 100, 2, 10, QLatin1Char('0'));
}

} // namespace domain
