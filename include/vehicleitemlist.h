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

    // Métodos para setear datos
    void setImage(const QPixmap& pix);
    void setModel(const QString& model);
    void setStatus(const QString& status);
    void setPrice(const QString& price);
    void setYear(const QString& year);
    void setColor(const QString& color);
    void setKm(const QString& km);
    void setTransmission(const QString& transmission);
    void setMotor(const QString& motor);
    void setDate(const QString& date);

private:
    Ui::VehicleItemList *ui;
};

#endif // VEHICLEITEMLIST_H
