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

void VehicleItemList::setImage(const QPixmap& pix) {
    //ui->imageLabel->setScaledContents(true);
    //ui->imageLabel->setPixmap(pix);
    ui->vehicleImage->setPixmap(pix.scaled(ui->vehicleImage->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
}

void VehicleItemList::setModel(const QString& model) {
    ui->vehicleTitle->setText(model);
}

void VehicleItemList::setStatus(const QString& status) {
    ui->statusBadge->setText(status);
}

void VehicleItemList::setPrice(const QString& price) {
    ui->priceLabel->setText(price);
}

void VehicleItemList::setYear(const QString& year) {
    ui->yearLabel->setText(year);
}

void VehicleItemList::setColor(const QString& color) {
    ui->colorLabel->setText(color);
}

void VehicleItemList::setKm(const QString& km) {
    ui->kmLabel->setText(km);
}

void VehicleItemList::setTransmission(const QString& transmission) {
    ui->transmisionLabel->setText(transmission);
}

void VehicleItemList::setMotor(const QString& motor) {
    ui->motorLabel->setText(motor);
}

void VehicleItemList::setDate(const QString& date) {
    ui->fechaIngresoLabel->setText(date);
}
