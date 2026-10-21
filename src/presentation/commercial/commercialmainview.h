//
// Created by Alucard on 14/10/2026.
//

#ifndef DESKTOP_WIN_CAR_DEALERSHIP_COMMERCIALMAINVIEW_H
#define DESKTOP_WIN_CAR_DEALERSHIP_COMMERCIALMAINVIEW_H

#include <QWidget>
#include "presentation/common/components/navtabitem.h"


QT_BEGIN_NAMESPACE

namespace Ui
{
    class CommercialMainView;
}

QT_END_NAMESPACE

class CommercialMainView : public QWidget
{
    Q_OBJECT

public:
    explicit CommercialMainView(QWidget* parent = nullptr);
    ~CommercialMainView() override;

private:
    Ui::CommercialMainView* ui;
};


#endif //DESKTOP_WIN_CAR_DEALERSHIP_COMMERCIALMAINVIEW_H
