#include "application/commercial/sales/services/saleservice.h"

#include <QSet>
#include <QtMath>

namespace application {
namespace {

constexpr auto kDuplicateNationalIdMessage =
    QT_TRANSLATE_NOOP("application", "Ya existe una persona registrada con ese número de identificación.");

SaleRegistrationResultDto rejected(const QString &field, const QString &message)
{
    SaleRegistrationResultDto result;
    result.status = SaleRegistrationResultDto::Status::Rejected;
    result.field = field;
    result.message = message;
    return result;
}

bool validMoney(double value)
{
    return qIsFinite(value) && value >= 0.0;
}

bool hasExistingNationalIdMatch(const QList<application::CounterpartyDto> &matches,
                                const QString &nationalId,
                                int currentCounterpartyId = -1)
{
    const QString normalized = nationalId.trimmed();
    if (normalized.isEmpty())
        return false;

    for (const application::CounterpartyDto &person : matches) {
        if (person.id == currentCounterpartyId)
            continue;

        const QString existing = person.nationalId.trimmed();
        if (existing.compare(normalized, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace

SaleService::SaleService(SaleRepository &sales, CounterpartyDirectory &counterparties)
    : m_sales(sales)
    , m_counterparties(counterparties)
{
}

SaleSearchResultDto SaleService::searchAvailableVehicles(const QString &text) const
{
    SaleSearchResultDto result;
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty() && trimmed.size() < 3)
        return result;

    result.vehicles = m_sales.searchAvailableVehicles(trimmed, &result.errorMessage);
    return result;
}

SaleVendorSearchResultDto SaleService::searchVendors(const QString &text) const
{
    SaleVendorSearchResultDto result;
    const QString query = text.trimmed();
    if (!query.isEmpty() && query.size() < 3)
        return result;

    result.vendors = m_sales.searchVendors(query, &result.errorMessage);
    return result;
}

SaleCounterpartySearchResultDto SaleService::searchCounterparties(const QString &text) const
{
    SaleCounterpartySearchResultDto result;
    const QString query = text.trimmed();
    if (query.size() < 3)
        return result;

    result.counterparties = m_counterparties.search(query, &result.errorMessage);
    return result;
}

CounterpartyUpdateResultDto SaleService::updateCounterparty(
    const CounterpartyDto &counterparty) const
{
    if (counterparty.id <= 0) {
        return {false, QStringLiteral("counterparty"),
                QStringLiteral("Selecciona una persona existente para actualizarla.")};
    }
    if (counterparty.fullName.trimmed().isEmpty() || counterparty.nationalId.trimmed().isEmpty()) {
        return {false, QStringLiteral("counterparty"),
                QStringLiteral("El nombre y la identificación son obligatorios.")};
    }
    return m_counterparties.update(counterparty);
}

SaleRegistrationResultDto SaleService::registerSale(const SaleRegistrationDto &sale) const
{
    const domain::ValidationResult requiredFields = validateSale(sale);
    if (!requiredFields.isValid())
        return rejected(requiredFields.errors().first().field, requiredFields.firstMessage());

    if (!validMoney(sale.subtotal) || !validMoney(sale.iva) || !validMoney(sale.totalAmount))
        return rejected(QStringLiteral("cost"), QStringLiteral("Los importes deben ser valores válidos y no negativos."));

    const QString buyerNationalId = sale.buyer.nationalId.trimmed();
    if (!buyerNationalId.isEmpty() && sale.buyer.counterpartyId <= 0) {
        QString locationError;
        const QList<application::CounterpartyDto> matches = m_counterparties.search(buyerNationalId, &locationError);
        if (!locationError.isEmpty())
            return rejected(QStringLiteral("buyer.nationalId"), locationError);
        if (hasExistingNationalIdMatch(matches, buyerNationalId))
            return rejected(QStringLiteral("buyer.nationalId"), QStringLiteral("Ya existe una persona registrada con ese número de identificación."));
    }

    if (sale.paymentType == domain::PaymentType::Credito && sale.financing && sale.financing->guarantor) {
        const SaleBuyerDto &guarantor = *sale.financing->guarantor;
        const QString guarantorNationalId = guarantor.nationalId.trimmed();
        if (!guarantorNationalId.isEmpty() && guarantor.counterpartyId <= 0) {
            QString guarantorError;
            const QList<application::CounterpartyDto> matches = m_counterparties.search(guarantorNationalId, &guarantorError);
            if (!guarantorError.isEmpty())
                return rejected(QStringLiteral("guarantor.nationalId"), guarantorError);
            if (hasExistingNationalIdMatch(matches, guarantorNationalId))
                return rejected(QStringLiteral("guarantor.nationalId"), QStringLiteral("Ya existe una persona registrada con ese número de identificación."));
        }
    }

    if (sale.paymentType == domain::PaymentType::Contado) {
        if (!sale.paymentMethod)
            return rejected(QStringLiteral("paymentMethod"), QStringLiteral("Selecciona el método de pago."));
        if (sale.financing)
            return rejected(QStringLiteral("financing"), QStringLiteral("Una venta de contado no debe incluir financiamiento."));
    } else {
        if (sale.paymentMethod)
            return rejected(QStringLiteral("paymentMethod"), QStringLiteral("El método de pago de contado no aplica a una venta a crédito."));
        if (!sale.financing)
            return rejected(QStringLiteral("financing"), QStringLiteral("Faltan los datos de financiamiento."));

        const SaleFinancingDto &financing = *sale.financing;
        if (!validMoney(financing.downPayment) || !validMoney(financing.amountToFinance)
            || !validMoney(financing.interestRatePercent) || !validMoney(financing.monthlyPayment)
            || !validMoney(financing.finalPrice) || financing.monthsTerm <= 0) {
            return rejected(QStringLiteral("financing"), QStringLiteral("Revisa los datos capturados de financiamiento."));
        }
        if (financing.downPayment > sale.totalAmount
            || qAbs(financing.downPayment + financing.amountToFinance - sale.totalAmount) > 0.01) {
            return rejected(QStringLiteral("financing"),
                            QStringLiteral("El enganche y el monto financiado deben sumar el precio de venta."));
        }
        if (financing.installments.size() != financing.monthsTerm)
            return rejected(QStringLiteral("installments"), QStringLiteral("La proyección debe contener una cuota por cada mes."));
        if (financing.installments.isEmpty())
            return rejected(QStringLiteral("installments"), QStringLiteral("Calcula la proyección antes de guardar la venta."));

        QSet<int> installmentNumbers;
        double installmentTotal = 0.0;
        for (const SaleInstallmentDto &installment : financing.installments) {
            if (installment.number <= 0 || installmentNumbers.contains(installment.number)
                || !installment.dueDate.isValid() || !validMoney(installment.expectedAmount)
                || !validMoney(installment.capitalAllocation) || !validMoney(installment.interestAllocation)
                || !validMoney(installment.remainingBalance)) {
                return rejected(QStringLiteral("installments"), QStringLiteral("La proyección contiene una cuota inválida."));
            }
            installmentNumbers.insert(installment.number);
            installmentTotal += installment.expectedAmount;
        }
        if (qAbs(financing.finalPrice - financing.downPayment - installmentTotal) > 0.01) {
            return rejected(QStringLiteral("installments"),
                            QStringLiteral("El precio final no coincide con la proyección de pagos."));
        }
    }

    return m_sales.registerSale(sale);
}

domain::ValidationResult SaleService::validateSale(const SaleRegistrationDto &sale) const
{
    domain::ValidationResult result;
    const auto require = [&result](const QString &field, const QString &value,
                                   const QString &message) {
        if (value.trimmed().isEmpty())
            result.addError(field, message);
    };

    if (sale.vehicleFolio <= 0)
        result.addError(QStringLiteral("vehicle"), QStringLiteral("Selecciona un vehículo válido."));
    if (sale.vendorId <= 0)
        result.addError(QStringLiteral("vendor"), QStringLiteral("Selecciona un vendedor."));
    if (!sale.saleDate.isValid())
        result.addError(QStringLiteral("date"), QStringLiteral("Captura una fecha válida."));

    require(QStringLiteral("buyer.fullName"), sale.buyer.fullName,
            QStringLiteral("Captura el nombre completo del comprador."));
    require(QStringLiteral("buyer.streetAddress"), sale.buyer.streetAddress,
            QStringLiteral("Captura el domicilio del comprador."));
    require(QStringLiteral("buyer.suburb"), sale.buyer.suburb,
            QStringLiteral("Captura la colonia del comprador."));
    require(QStringLiteral("buyer.locality"), sale.buyer.locality,
            QStringLiteral("Captura la localidad del comprador."));
    require(QStringLiteral("buyer.state"), sale.buyer.state,
            QStringLiteral("Captura el estado del comprador."));
    require(QStringLiteral("buyer.postalCode"), sale.buyer.postalCode,
            QStringLiteral("Captura el código postal del comprador."));
    require(QStringLiteral("buyer.phone"), sale.buyer.phone,
            QStringLiteral("Captura el teléfono del comprador."));
    require(QStringLiteral("buyer.email"), sale.buyer.email,
            QStringLiteral("Captura el correo electrónico del comprador."));
    require(QStringLiteral("buyer.nationalId"), sale.buyer.nationalId,
            QStringLiteral("Captura la identificación del comprador. XD"));

    if (sale.updateBuyerRecord && sale.buyer.counterpartyId <= 0) {
        result.addError(QStringLiteral("buyer.fullName"),
                        QStringLiteral("Selecciona un comprador existente para actualizar sus datos."));
    }

    if (sale.paymentType == domain::PaymentType::Credito) {
        if (!sale.financing || !sale.financing->guarantor) {
            result.addError(QStringLiteral("guarantor.fullName"),
                            QStringLiteral("Captura la información completa del aval."));
        } else {
            const SaleBuyerDto &guarantor = *sale.financing->guarantor;
            require(QStringLiteral("guarantor.fullName"), guarantor.fullName,
                    QStringLiteral("Captura el nombre completo del aval."));
            require(QStringLiteral("guarantor.nationalId"), guarantor.nationalId,
                    QStringLiteral("Captura la identificación del aval."));
            require(QStringLiteral("guarantor.streetAddress"), guarantor.streetAddress,
                    QStringLiteral("Captura el domicilio del aval."));
            require(QStringLiteral("guarantor.suburb"), guarantor.suburb,
                    QStringLiteral("Captura la colonia del aval."));
            require(QStringLiteral("guarantor.locality"), guarantor.locality,
                    QStringLiteral("Captura la localidad del aval."));
            require(QStringLiteral("guarantor.state"), guarantor.state,
                    QStringLiteral("Captura el estado del aval."));
            require(QStringLiteral("guarantor.postalCode"), guarantor.postalCode,
                    QStringLiteral("Captura el código postal del aval."));
            require(QStringLiteral("guarantor.phone"), guarantor.phone,
                    QStringLiteral("Captura el teléfono del aval."));
            require(QStringLiteral("guarantor.email"), guarantor.email,
                    QStringLiteral("Captura el correo electrónico del aval."));
            if (sale.updateGuarantorRecord && guarantor.counterpartyId <= 0) {
            result.addError(QStringLiteral("guarantor.fullName"),
                                QStringLiteral("Selecciona un aval existente para actualizar sus datos."));
            }
        }
    }
    return result;
}

} // namespace application
