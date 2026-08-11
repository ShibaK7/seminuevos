#include "../include/vehicleitemlist.h"
#include "../ui/ui_vehicleitemlist.h"

VehicleItemList::VehicleItemList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VehicleItemList)
{
    ui->setupUi(this);
}

VehicleItemList::~VehicleItemList()
{
    delete ui;
}
