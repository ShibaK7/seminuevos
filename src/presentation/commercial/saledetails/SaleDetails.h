#ifndef PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILS_H
#define PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILS_H

#include "domain/common/value_objects/enums.h"
#include "presentation/commercial/saledetails/isaledetailsview.h"

#include <QPointer>
#include <QWidget>

class AddPaymentDialog;

QT_BEGIN_NAMESPACE
namespace Ui {
class SaleDetails;
}
QT_END_NAMESPACE

namespace presentation {
class CommercialSaleDetailsPresenter;
}

class SaleDetails final : public QWidget, public presentation::ICommercialSaleDetailsView
{
    Q_OBJECT

public:
    explicit SaleDetails(domain::UserRole role, QWidget *parent = nullptr);
    ~SaleDetails() override;

    void bind(presentation::CommercialSaleDetailsPresenter &presenter);
    void requestFullDetails();
    void showSaleDetails(const application::CommercialSaleDetailsDto &details) override;
    void showNextPaymentEntry(
        const application::CommercialPaymentLookupResultDto &result) override;
    void paymentEntrySaved() override;
    void showPaymentEntrySaveError(const QString &message) override;
    void showFullSaleDetails(
        const application::CommercialSaleRegistrationDetailsDto &details) override;
    void showSaleDetailsError(const QString &message) override;

signals:
    void returnToCommercial();
    void paymentWaiverChanged(int paymentFolio, bool waived);
    void paymentEntryRequested(int saleFolio);
    void paymentEntrySubmitted(const application::CommercialPaymentDto &payment);
    void paymentPrintRequested(const application::CommercialSaleDetailsDto &sale,
                               const application::CommercialPaymentDto &payment);
    void fullDetailsRequested();
    void fullSaleDetailsLoaded(
        const application::CommercialSaleRegistrationDetailsDto &details);

private:
    void onFullDetailsRequested();
    void onUpdateFinancingRequested();
    void onAddPaymentRequested();
    void onPrintRequested();

    Ui::SaleDetails *ui;
    const domain::UserRole m_role;
    QPointer<presentation::CommercialSaleDetailsPresenter> m_presenter;
    QPointer<AddPaymentDialog> m_paymentDialog;
    application::CommercialSaleDetailsDto m_saleDetails;
};

#endif // PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILS_H
