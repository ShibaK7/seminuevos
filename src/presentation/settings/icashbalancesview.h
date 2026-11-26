#ifndef PRESENTATION_SETTINGS_ICASHBALANCESVIEW_H
#define PRESENTATION_SETTINGS_ICASHBALANCESVIEW_H

#include "application/finance/dto/cashdtos.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>

#include <optional>

namespace presentation {

// La tarjeta de saldos de Configuraciones (Caja Chica, Cantidad Banco y Caja
// Fuerte), vista desde su presenter. La implementa CashBalancesWidget.
class ICashBalancesView
{
public:
    virtual ~ICashBalancesView() = default;

    // Mientras se lee o se guarda algo: los botones no responden.
    virtual void setBusy(bool busy) = 0;
    // Los tres saldos, en el orden de pantalla. Con ellos decide qué botones
    // ofrece (los saldos iniciales solo mientras falte capturarlos) y, si hay
    // un formulario abierto, su máximo y su "Disponible".
    virtual void showBalances(const QList<application::CashBalanceDto> &balances) = 0;
    virtual void showMovements(const QList<application::CashMovementDto> &movements) = 0;
    // La cuenta elegida en el filtro de movimientos; nullopt: todas.
    virtual std::optional<domain::CashAccount> movementFilter() const = 0;

    // --- El formulario (modal; lo capturado llega después, con una señal) --
    virtual void askOpening() = 0;
    // El origen se elige en el formulario; su máximo es lo que hay en él.
    virtual void askSafeDeposit(const QList<application::CashBalanceDto> &balances) = 0;
    // El máximo es lo que hay en Caja Fuerte.
    virtual void askSafeWithdrawal(qint64 availableHundredths) = 0;
    // Los saldos iniciales no se pueden volver a capturar: se confirma antes.
    virtual bool confirmOpening(const application::OpeningBalancesDto &openings) = 0;
    // Mientras se guarda: el formulario no se cierra ni se vuelve a enviar.
    virtual void setFormBusy(bool busy) = 0;
    virtual void showFormErrors(const QList<domain::ValidationError> &errors) = 0;
    virtual void closeForm() = 0;

    virtual void showMessage(const QString &message) = 0;
    virtual void showNotice(const QString &notice) = 0;

protected:
    ICashBalancesView() = default;
    ICashBalancesView(const ICashBalancesView &) = default;
    ICashBalancesView &operator=(const ICashBalancesView &) = default;
};

} // namespace presentation

#endif // PRESENTATION_SETTINGS_ICASHBALANCESVIEW_H
