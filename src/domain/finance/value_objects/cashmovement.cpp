#include "domain/finance/value_objects/cashmovement.h"

#include <QList>

#include <cmath>
#include <cstdlib>

namespace domain {

namespace {

const QList<CashMovementKind> &allKinds()
{
    static const QList<CashMovementKind> kinds{
        CashMovementKind::Opening,  CashMovementKind::Purchase,   CashMovementKind::PurchaseCorrection,
        CashMovementKind::Sale,     CashMovementKind::DownPayment, CashMovementKind::Transfer,
        CashMovementKind::Withdrawal, CashMovementKind::Maintenance, CashMovementKind::Expense,
    };
    return kinds;
}

} // namespace

QString toDbString(CashMovementKind kind)
{
    switch (kind) {
    case CashMovementKind::Opening:
        return QStringLiteral("Apertura");
    case CashMovementKind::Purchase:
        return QStringLiteral("Compra");
    case CashMovementKind::PurchaseCorrection:
        return QStringLiteral("Corrección de compra");
    case CashMovementKind::Sale:
        return QStringLiteral("Venta");
    case CashMovementKind::DownPayment:
        return QStringLiteral("Enganche");
    case CashMovementKind::Transfer:
        return QStringLiteral("Traspaso");
    case CashMovementKind::Withdrawal:
        return QStringLiteral("Retiro");
    case CashMovementKind::Maintenance:
        return QStringLiteral("Mantenimiento");
    case CashMovementKind::Expense:
        return QStringLiteral("Gasto");
    }
    return QString();
}

std::optional<CashMovementKind> cashMovementKindFromDb(const QString &raw)
{
    for (const CashMovementKind kind : allKinds()) {
        if (toDbString(kind) == raw)
            return kind;
    }
    return std::nullopt;
}

CashMovement::CashMovement(CashAccount account, qint64 hundredths, CashMovementKind kind,
                           const QString &concept, const QDate &date)
    : m_account(account)
    , m_hundredths(hundredths)
    , m_kind(kind)
    , m_concept(concept)
    , m_date(date)
{
}

CashMovement CashMovement::inflow(CashAccount account, qint64 hundredths, CashMovementKind kind,
                                  const QString &concept, const QDate &date)
{
    return CashMovement(account, std::llabs(hundredths), kind, concept, date);
}

CashMovement CashMovement::outflow(CashAccount account, qint64 hundredths, CashMovementKind kind,
                                   const QString &concept, const QDate &date)
{
    return CashMovement(account, -std::llabs(hundredths), kind, concept, date);
}

CashAccount CashMovement::account() const
{
    return m_account;
}

qint64 CashMovement::hundredths() const
{
    return m_hundredths;
}

bool CashMovement::isInflow() const
{
    return m_hundredths > 0;
}

CashMovementKind CashMovement::kind() const
{
    return m_kind;
}

QString CashMovement::concept() const
{
    return m_concept;
}

QDate CashMovement::date() const
{
    return m_date;
}

bool CashMovement::operator==(const CashMovement &other) const
{
    return m_account == other.m_account && m_hundredths == other.m_hundredths
           && m_kind == other.m_kind && m_concept == other.m_concept && m_date == other.m_date;
}

qint64 toHundredths(double pesos)
{
    return std::llround(pesos * 100.0);
}

} // namespace domain
