#include "application/commercial/services/commercialservice.h"

namespace application {

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

} // namespace application
