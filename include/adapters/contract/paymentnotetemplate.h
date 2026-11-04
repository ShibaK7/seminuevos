#ifndef ADAPTERS_CONTRACT_PAYMENTNOTETEMPLATE_H
#define ADAPTERS_CONTRACT_PAYMENTNOTETEMPLATE_H

#include "application/commercial/dto/commercialdtos.h"

#include <QString>

class PaymentNoteTemplate
{
public:
    static QString fillTemplate(const application::CommercialSaleDetailsDto &sale,
                                const application::CommercialPaymentDto &payment,
                                QString *error);
    static QString suggestedFileName(const application::CommercialPaymentDto &payment);
    static bool writePdf(const QString &html, const QString &outputPath, QString *error);
};

#endif // ADAPTERS_CONTRACT_PAYMENTNOTETEMPLATE_H
