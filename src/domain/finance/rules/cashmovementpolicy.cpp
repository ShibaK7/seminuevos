#include "domain/finance/rules/cashmovementpolicy.h"

#include <QLocale>

namespace domain {

QString formatPesos(qint64 hundredths)
{
    const QLocale mexico(QLocale::Spanish, QLocale::Mexico);
    return QStringLiteral("$%1").arg(mexico.toString(static_cast<double>(hundredths) / 100.0, 'f', 2));
}

QString CashMovementPolicy::openingField(CashAccount account)
{
    switch (account) {
    case CashAccount::PettyCash:
        return QStringLiteral("pettyCashOpening");
    case CashAccount::Bank:
        return QStringLiteral("bankOpening");
    case CashAccount::Safe:
        return QStringLiteral("safeOpening");
    }
    return QString();
}

QString CashMovementPolicy::sourceField()
{
    return QStringLiteral("source");
}

QString CashMovementPolicy::amountField()
{
    return QStringLiteral("amount");
}

QString CashMovementPolicy::dateField()
{
    return QStringLiteral("date");
}

QString CashMovementPolicy::conceptField()
{
    return QStringLiteral("concept");
}

void CashMovementPolicy::checkOpening(CashAccount account, qint64 hundredths, ValidationResult &result)
{
    const QString field = openingField(account);
    if (account == CashAccount::Safe)
        result.addError(field, QStringLiteral("Caja Fuerte empieza en cero: se llena con ingresos."));
    else if (hundredths < 0)
        result.addError(field, QStringLiteral("El saldo inicial no puede ser negativo."));
    else if (hundredths > kMaxHundredths)
        result.addError(field, QStringLiteral("El saldo no puede pasar de $9,999,999,999.99."));
}

void CashMovementPolicy::checkWithin(qint64 hundredths, CashAccount from, qint64 available,
                                     ValidationResult &result)
{
    if (hundredths <= 0)
        result.addError(amountField(), QStringLiteral("Captura un monto mayor que cero."));
    else if (hundredths > available)
        result.addError(amountField(), insufficientFundsMessage(from, available));
}

void CashMovementPolicy::checkSafeDeposit(CashAccount source, qint64 hundredths, qint64 available,
                                          ValidationResult &result)
{
    if (source == CashAccount::Safe) {
        result.addError(sourceField(), QStringLiteral("El ingreso a Caja Fuerte sale de Caja Chica o "
                                                      "de Cantidad Banco."));
        return;
    }
    checkWithin(hundredths, source, available, result);
}

void CashMovementPolicy::checkSafeWithdrawal(const QDate &date, const QDate &today,
                                             const QString &concept, qint64 hundredths,
                                             qint64 available, ValidationResult &result)
{
    if (!date.isValid())
        result.addError(dateField(), QStringLiteral("Captura cuándo salió el dinero."));
    else if (date > today)
        result.addError(dateField(), QStringLiteral("La fecha no puede ser posterior a hoy."));

    const QString trimmed = concept.trimmed();
    if (trimmed.isEmpty())
        result.addError(conceptField(), QStringLiteral("Captura el concepto del retiro."));
    else if (trimmed.size() > kMaxConceptLength)
        result.addError(conceptField(), QStringLiteral("El concepto no puede pasar de %1 caracteres.")
                                            .arg(kMaxConceptLength));

    checkWithin(hundredths, CashAccount::Safe, available, result);
}

QString CashMovementPolicy::insufficientFundsMessage(CashAccount account, qint64 available)
{
    return QStringLiteral("%1 solo tiene %2 disponibles.").arg(displayLabel(account), formatPesos(available));
}

} // namespace domain
