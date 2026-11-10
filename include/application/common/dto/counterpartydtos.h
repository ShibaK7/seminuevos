#ifndef APPLICATION_COMMON_DTO_COUNTERPARTYDTOS_H
#define APPLICATION_COMMON_DTO_COUNTERPARTYDTOS_H

#include <QList>
#include <QString>

namespace application {

// A shared person record. Its role (buyer, guarantor, vehicle seller/owner)
// comes from the relationship that references the counterparty ID.
struct CounterpartyDto
{
    int id = -1;
    QString fullName;
    QString nationalId;
    QString streetAddress;
    QString suburb;
    QString locality;
    QString state;
    QString postalCode;
    QString phone;
    QString email;
};

struct CounterpartySearchResultDto
{
    QList<CounterpartyDto> counterparties;
    QString errorMessage;
};

struct CounterpartyUpdateResultDto
{
    bool ok = false;
    QString field;
    QString message;
};

} // namespace application

#endif // APPLICATION_COMMON_DTO_COUNTERPARTYDTOS_H
