#ifndef PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILS_H
#define PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILS_H

#include "domain/common/value_objects/enums.h"
#include "presentation/commercial/saledetails/isaledetailsview.h"

#include <QPointer>
#include <QWidget>

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
    void showFullSaleDetails(
        const application::CommercialSaleRegistrationDetailsDto &details) override;
    void showSaleDetailsError(const QString &message) override;

signals:
    void returnToCommercial();
    void paymentWaiverChanged(int paymentFolio, bool waived);
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
};

#endif // PRESENTATION_COMMERCIAL_SALEDETAILS_SALEDETAILS_H
