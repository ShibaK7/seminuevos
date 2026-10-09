#ifndef PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSVIEW_H
#define PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSVIEW_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "presentation/inventory/registration/steps/details/ivehicledetailsview.h"

#include <QList>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class VehicleDetailsView;
}
QT_END_NAMESPACE

// Paso 1 del wizard: datos generales del vehículo + contraparte + condiciones
// de la operación. Cubre las dos ramas -- Adquisición (la agencia compra) y
// Consignación (la agencia vende por cuenta del dueño y cobra comisión). Lo
// que cambia entre ramas vive en su sección (inventory/acquisition/ e
// inventory/consignment/), promovida dentro de este formulario: el selector de
// tipo muestra una y oculta la otra.
//
// Vista pasiva (IVehicleDetailsView): entrega lo capturado como
// VehicleDetailsDto y pinta lo que VehicleDetailsPresenter decide. Cuáles
// campos son obligatorios lo decide el dominio; la factura adjunta y la regla
// de la autofactura, el presenter. Cada widget de captura lleva en la
// propiedad dinámica "field" la clave con la que el dominio reporta sus
// errores, y con ella se marcan los que fallan.
//
// Las dos tarjetas y todos sus campos están en vehicledetailsview.ui (en esta misma carpeta);
// la propiedad "field" de cada uno también se edita ahí.
class VehicleDetailsView : public QWidget, public presentation::IVehicleDetailsView
{
    Q_OBJECT

public:
    explicit VehicleDetailsView(QWidget *parent = nullptr);
    ~VehicleDetailsView() override;

    // --- IVehicleDetailsView ---
    application::VehicleDetailsDto details() const override;
    void setLookups(const application::RegistrationLookupsDto &lookups) override;
    presentation::IAcquisitionTermsView &acquisitionTerms() override;
    void showFieldErrors(const QList<domain::ValidationError> &errors) override;
    bool focusField(const QString &field) override;

signals:
    // Cambió cualquier campo.
    void edited();
    void invoiceTypeChanged();
    // Los botones de la factura, que viven en la sección de Adquisición. Se
    // reenvían aquí para que el asistente no tenga que conocer la sección.
    void browseInvoiceRequested();
    void cfdiRequestRequested();

protected:
    // Alinea las columnas de las secciones con las de la tarjeta, la primera
    // vez que se muestra (ya pulida la hoja de estilos).
    void showEvent(QShowEvent *event) override;

private slots:
    void reloadSubtypes();
    // Muestra los campos de la rama elegida y repuebla el combo de factura.
    void onAcquisitionTypeChanged();

private:
    domain::AcquisitionType selectedAcquisitionType() const;

    Ui::VehicleDetailsView *ui;

    bool m_columnsAligned = false;

    // Todos los subtipos con su tipo padre; reloadSubtypes() filtra.
    QList<application::CatalogOptionDto> m_subtypes;
};

#endif // PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSVIEW_H
