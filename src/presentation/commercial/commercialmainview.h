//
// Created by Alucard on 14/10/2026.
//

#ifndef DESKTOP_WIN_CAR_DEALERSHIP_COMMERCIALMAINVIEW_H
#define DESKTOP_WIN_CAR_DEALERSHIP_COMMERCIALMAINVIEW_H

#include <QWidget>
#include <QTimer>
#include "presentation/commercial/icommercialview.h"
#include "presentation/common/components/navtabitem.h"

namespace presentation {
class CommercialPresenter;
}

QT_BEGIN_NAMESPACE

namespace Ui
{
    class CommercialMainView;
}

QT_END_NAMESPACE

class CommercialMainView : public QWidget, public presentation::ICommercialView
{
    Q_OBJECT

public:
    explicit CommercialMainView(QWidget* parent = nullptr);
    ~CommercialMainView() override;

    void bind(presentation::CommercialPresenter &presenter);
    application::CommercialFilterDto filter() const override;
    void showSales(const QList<application::CommercialSaleDto> &sales) override;
    void showCommercialMessage(const QString &message) override;

signals:
    void saleDetailsRequested(int folio);
    void fullSaleDetailsRequested(int folio);
    void newSaleRequested();

private:
    void setCollectionsMode(bool enabled, presentation::CommercialPresenter *presenter = nullptr);
    void configureTableColumns();

    Ui::CommercialMainView* ui;
    QTimer m_searchTimer;
    bool m_collectionsMode = false;
};


#endif //DESKTOP_WIN_CAR_DEALERSHIP_COMMERCIALMAINVIEW_H
