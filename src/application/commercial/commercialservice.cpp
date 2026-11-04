#include "application/commercial/services/commercialservice.h"

#include <cmath>
#include <QStringList>

namespace application {
namespace {

bool validMoney(double amount)
{
    if (!std::isfinite(amount) || amount > 9999999999.99)
        return false;
    return std::abs(amount * 100.0 - std::round(amount * 100.0)) < 0.001;
}

} // namespace

CommercialService::CommercialService(CommercialReader &reader, CommercialPaymentWriter &writer)
    : m_reader(reader)
    , m_writer(writer)
{
}

CommercialResultDto CommercialService::search(const CommercialFilterDto &filter) const
{
    CommercialResultDto result;
    result.sales = m_reader.search(filter, &result.errorMessage);
    return result;
}

CommercialSaleDetailsResultDto CommercialService::saleDetails(int folio) const
{
    CommercialSaleDetailsResultDto result;
    result.sale = m_reader.saleDetails(folio, &result.errorMessage);
    return result;
}

CommercialPaymentLookupResultDto CommercialService::nextUnpaidPayment(int saleFolio) const
{
    CommercialPaymentLookupResultDto result;
    if (saleFolio <= 0) {
        result.errorMessage = QStringLiteral("El folio de venta no es válido.");
        return result;
    }

    result = m_reader.nextUnpaidPayment(saleFolio);
    result.found = result.found && result.errorMessage.isEmpty()
                   && result.payment.folio > 0;
    return result;
}

CommercialSaleRegistrationDetailsResultDto CommercialService::saleRegistrationDetails(
    int folio) const
{
    CommercialSaleRegistrationDetailsResultDto result;
    result.sale = m_reader.saleRegistrationDetails(folio, &result.errorMessage);
    return result;
}

CommercialWriteResultDto CommercialService::updatePaymentWaiver(
    int paymentFolio, bool waived, domain::UserRole role) const
{
    CommercialWriteResultDto result;
    if (role != domain::UserRole::Admin) {
        result.errorMessage = QStringLiteral("Solo un Administrador puede modificar condonaciones.");
        return result;
    }

    result.ok = m_writer.updatePaymentWaiver(
        paymentFolio, waived, role, &result.errorMessage);
    return result;
}

CommercialWriteResultDto CommercialService::recordPayment(
    const CommercialPaymentDto &payment) const
{
    CommercialWriteResultDto result;
    const QDate today = QDate::currentDate();
    const QStringList validStatuses = {
        QStringLiteral("Pendiente"), QStringLiteral("Pagado"),
        QStringLiteral("Parcial"), QStringLiteral("Atrasado")
    };

    if (payment.folio <= 0 || payment.saleFolio <= 0 || payment.installmentNumber <= 0) {
        result.errorMessage = QStringLiteral("No se identificó correctamente la cuota de pago.");
    } else if (!payment.paidDate.isValid()
               || payment.paidDate < today.addMonths(-2) || payment.paidDate > today) {
        result.errorMessage = QStringLiteral("La fecha de pago debe estar entre hoy y los últimos dos meses.");
    } else if (!validMoney(payment.paidAmount) || payment.paidAmount <= 0.0) {
        result.errorMessage = QStringLiteral(
            "El monto pagado debe ser mayor que cero y tener hasta dos decimales.");
    } else if (!validMoney(payment.lateFee) || payment.lateFee < 0.0) {
        result.errorMessage = QStringLiteral(
            "El recargo debe ser mayor o igual a cero y tener hasta dos decimales.");
    } else if (payment.paymentMethod.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("La forma de pago es obligatoria.");
    } else if (payment.paymentMethod.trimmed().size() > 30) {
        result.errorMessage = QStringLiteral("La forma de pago no puede exceder 30 caracteres.");
    } else if (!validStatuses.contains(payment.status)) {
        result.errorMessage = QStringLiteral("Selecciona un estado de pago válido.");
    } else if (payment.referenceNumber.trimmed().size() > 100) {
        result.errorMessage = QStringLiteral("La referencia no puede exceder 100 caracteres.");
    }

    if (!result.errorMessage.isEmpty())
        return result;

    result.ok = m_writer.recordPayment(payment, &result.errorMessage);
    return result;
}

} // namespace application
