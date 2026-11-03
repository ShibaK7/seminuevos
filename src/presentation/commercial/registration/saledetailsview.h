#ifndef SALEDETAILSVIEW_H
#define SALEDETAILSVIEW_H

#include "application/common/dto/counterpartydtos.h"
#include "application/auth/dto/authdtos.h"
#include "application/commercial/dto/commercialdtos.h"
#include "application/commercial/sales/dto/saledtos.h"
#include "domain/common/value_objects/enums.h"
#include "presentation/commercial/registration/isaledetailsview.h"
#include <QEvent>
#include <QList>
#include <QWidget>

class QCompleter;

namespace Ui {
class SaleDetailsView;
}

class SaleDetailsView : public QWidget, public presentation::ISaleDetailsView
{
    Q_OBJECT

public:
    explicit SaleDetailsView(QWidget *parent = nullptr);
    ~SaleDetailsView();

    void setCounterpartyMatches(bool guarantor,
                                const QList<application::CounterpartyDto> &matches) override;
    void showCounterpartySearchError(bool guarantor, const QString &message) override;
    void setVehicleMatches(const QList<application::SaleVehicleOptionDto> &matches) override;
    void showVehicleSearchError(const QString &message) override;
    void setVendorMatches(const QList<application::SaleVendorOptionDto> &matches) override;
    void showVendorSearchError(const QString &message) override;
    QList<domain::ValidationError> showSaleValidationErrors(
        const QList<domain::ValidationError> &errors) override;
    bool focusSaleField(const QString &field) override;
    void setCurrentUser(const application::SessionDto &session);
    void resetForm();
    void showSavedSale(const application::CommercialSaleRegistrationDetailsDto &details);
    int selectedVehicleFolio() const;
    application::SaleVehicleOptionDto selectedVehicle() const;
    application::SaleRegistrationDto registrationForm() const;
    application::CounterpartyDto counterpartyForm(bool guarantor) const;
    bool shouldUpdateCounterparty(bool guarantor) const;

signals:
    void counterpartySearchRequested(const QString &query, bool guarantor);
    void counterpartySelected(int counterpartyId, bool guarantor);
    void vehicleSearchRequested(const QString &query);
    void vehicleSelected(int vehicleFolio);
    void vendorSearchRequested(const QString &query);

private slots:
    void onPaymentTypeChanged();
    void calculatePaymentProjection();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    domain::PaymentType selectedPaymentType() const;
    void applyCounterparty(const application::CounterpartyDto &counterparty, bool guarantor);
    void selectCounterparty(const QString &displayText, bool guarantor);
    void resetVehicleSelection();
    void initializeFinancingDefaults();
    void invalidatePaymentProjection();
    void selectVendor(const QString &displayText);
    void applyCostCalculations(double vehiclePrice, bool isFinancing);
    void applyVehicle(const application::SaleVehicleOptionDto &vehicle);
    void selectVehicle(const QString &displayText);

    Ui::SaleDetailsView *ui;
    QList<QCompleter *> m_buyerCompleters;
    QList<QCompleter *> m_guarantorCompleters;
    QList<application::CounterpartyDto> m_buyerMatches;
    QList<application::CounterpartyDto> m_guarantorMatches;
    QList<application::SaleVehicleOptionDto> m_vehicleMatches;
    QList<application::SaleVendorOptionDto> m_vendorMatches;
    QList<application::SaleInstallmentDto> m_projectionInstallments;
    int m_buyerCounterpartyId = -1;
    int m_guarantorCounterpartyId = -1;
    int m_vehicleFolio = -1;
    application::SaleVehicleOptionDto m_selectedVehicle;
    bool m_financingInitialized = false;
    int m_vendorId = -1;
    bool m_vendorDropdownRequested = false;
    application::SessionDto m_currentUser;
    bool m_hasCurrentUser = false;
};

#endif // SALEDETAILSVIEW_H
