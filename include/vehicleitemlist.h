#ifndef VEHICLEITEMLIST_H
#define VEHICLEITEMLIST_H

#include <QWidget>

namespace Ui {
class VehicleItemList;
}

struct VehicleSummary;

// Una tarjeta del listado de inventario: foto, título, estado, precio y la
// ficha corta (año, color, kilometraje, transmisión, combustible, fecha de
// alta).
class VehicleItemList : public QWidget
{
    Q_OBJECT

public:
    explicit VehicleItemList(QWidget *parent = nullptr);
    ~VehicleItemList() override;

    // Vuelca un renglón del inventario sobre la tarjeta, haciendo el formateo
    // que le corresponde a la presentación: importes con separador de miles,
    // kilometraje con unidad, fechas legibles, y un guión donde el dato falte.
    void setSummary(const VehicleSummary &summary);

    void setImage(const QPixmap &pix);

signals:
    // El botón "Detalles" de la tarjeta. Lleva el folio porque la tarjeta no
    // conserva el resumen completo.
    void detailsRequested(int folio);

private:
    // Además del texto, ajusta la propiedad dinámica que el QSS usa para
    // colorear la insignia. Sin eso, el badge se queda con el verde del .ui
    // pase lo que pase -- incluso en una unidad vendida.
    void setStatus(const QString &label, const QString &styleKey);

    Ui::VehicleItemList *ui;
    int m_folio = -1;
};

#endif // VEHICLEITEMLIST_H
