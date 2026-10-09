#ifndef PRESENTATION_INVENTORY_CONSIGNMENT_CONSIGNMENTTERMSSECTION_H
#define PRESENTATION_INVENTORY_CONSIGNMENT_CONSIGNMENTTERMSSECTION_H

#include "application/inventory/registration/dto/registrationdtos.h"

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class ConsignmentTermsSection;
}
QT_END_NAMESPACE

class QGridLayout;

// Lo que solo se captura en una consignación, dentro del Paso 1: el precio
// base que se le entrega al propietario (obligatorio: también lo imprime el
// contrato) y la comisión de la agencia. No hay precio de venta que capturar:
// sale de base + comisión, igual que la columna generada de
// vehicle_consignments. Su formulario es consignmenttermssection.ui (en esta
// misma carpeta), y en el del Paso 1 aparece como widget promovido; el paso la
// muestra solo en Consignación.
//
// No tiene lógica propia: sus reglas son del dominio (ConsignedVehicle).
class ConsignmentTermsSection : public QWidget
{
    Q_OBJECT

public:
    explicit ConsignmentTermsSection(QWidget *parent = nullptr);
    ~ConsignmentTermsSection() override;

    // Copia al DTO del paso lo capturado en esta sección.
    void fill(application::VehicleDetailsDto &dto) const;

    // Sus campos en orden de tabulación, para que el paso los encadene con
    // los suyos.
    QList<QWidget *> focusOrder() const;
    // Su rejilla, para alinear sus columnas con las de la tarjeta del paso.
    QGridLayout *grid() const;

private:
    Ui::ConsignmentTermsSection *ui;
};

#endif // PRESENTATION_INVENTORY_CONSIGNMENT_CONSIGNMENTTERMSSECTION_H
