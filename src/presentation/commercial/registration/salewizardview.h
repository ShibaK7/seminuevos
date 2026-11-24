#ifndef SALEWIZARDVIEW_H
#define SALEWIZARDVIEW_H

#include <QWidget>

namespace application {
struct CommercialSaleRegistrationDetailsDto;
}

namespace Ui {
class SaleWizardView;
}

class SaleDetailsView;

class SaleWizardView : public QWidget
{
    Q_OBJECT

public:
    explicit SaleWizardView(QWidget *parent = nullptr);
    ~SaleWizardView();

    SaleDetailsView &detailsView();
    void showSavedSaleDetails(const application::CommercialSaleRegistrationDetailsDto &details);
    void setSaving(bool saving);
    void showSaveResult(bool success, int saleFolio, const QString &message);

signals:
    void saveSaleRequested();
    void returnToCommercial();

private slots:
    void onCancelClicked();
    void printContract();

private:
    Ui::SaleWizardView *ui;
    SaleDetailsView *m_detailsView = nullptr;
    bool m_isSavedSaleView = false;

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool confirmDiscard();
    void resetToInitialState();

    void setupNewSaleFlow();
    void setupSaleDetailsFlow(bool isAdmin);
    void setupCollectionFlow(bool isAdmin);
};

#endif // SALEWIZARDVIEW_H
