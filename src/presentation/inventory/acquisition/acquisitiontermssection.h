#ifndef PRESENTATION_INVENTORY_ACQUISITION_ACQUISITIONTERMSSECTION_H
#define PRESENTATION_INVENTORY_ACQUISITION_ACQUISITIONTERMSSECTION_H

#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/inventory/acquisition/iacquisitiontermsview.h"

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class AcquisitionTermsSection;
}
QT_END_NAMESPACE

class QGridLayout;

// Lo que solo se captura en una compra, dentro del Paso 1: el archivo de la
// factura (con la solicitud de CFDI de la autofactura), el precio de compra,
// el tipo y el método de pago y el precio de venta. Su formulario es
// acquisitiontermssection.ui (en esta misma carpeta), y en el del Paso 1
// aparece como widget promovido; el paso la muestra solo en Adquisición.
//
// Vista pasiva: los botones de la factura los atiende
// AcquisitionTermsPresenter, y las reglas de los precios y del tope de pago en
// efectivo son del dominio (AcquiredVehicle), que reporta cada error con la
// clave "field" del campo y el asistente lo marca.
class AcquisitionTermsSection : public QWidget, public presentation::IAcquisitionTermsView
{
    Q_OBJECT

public:
    explicit AcquisitionTermsSection(QWidget *parent = nullptr);
    ~AcquisitionTermsSection() override;

    // Copia al DTO del paso lo capturado en esta sección.
    void fill(application::VehicleDetailsDto &dto) const;

    // Sus campos en orden de tabulación, para que el paso los encadene con
    // los suyos (el orden cruza dos formularios).
    QList<QWidget *> focusOrder() const;
    // Su rejilla, para alinear sus columnas con las de la tarjeta del paso.
    QGridLayout *grid() const;

    // --- IAcquisitionTermsView ---
    void showInvoiceAttachment(const presentation::InvoiceAttachmentState &state) override;
    QString askInvoiceFile() override;
    bool openDocument(const QString &path) override;
    void showWarning(const QString &title, const QString &message) override;

signals:
    void browseInvoiceRequested();
    void cfdiRequestRequested();

private:
    Ui::AcquisitionTermsSection *ui;
};

#endif // PRESENTATION_INVENTORY_ACQUISITION_ACQUISITIONTERMSSECTION_H
