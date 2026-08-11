#ifndef VEHICLEITEMLIST_H
#define VEHICLEITEMLIST_H

#include <QWidget>

namespace Ui {
class VehicleItemList;
}

class VehicleItemList : public QWidget
{
    Q_OBJECT

public:
    explicit VehicleItemList(QWidget *parent = nullptr);
    ~VehicleItemList();

private:
    Ui::VehicleItemList *ui;
};

#endif // VEHICLEITEMLIST_H
