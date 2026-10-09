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
// Consignación (la agencia vende por cuenta del dueño y cobra comisión) --
// mostrando los campos de una u otra según el selector de tipo.
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
    void showInvoiceAttachment(const presentation::InvoiceAttachmentState &state) override;
    QString askInvoiceFile() override;
    bool openDocument(const QString &path) override;
    void showWarning(const QString &title, const QString &message) override;
    void showFieldErrors(const QList<domain::ValidationError> &errors) override;
    bool focusField(const QString &field) override;

signals:
    // Cambió cualquier campo.
    void edited();
    void invoiceTypeChanged();
    void browseInvoiceRequested();
    void cfdiRequestRequested();

private slots:
    void reloadSubtypes();
    // Muestra los campos de la rama elegida y repuebla el combo de factura.
    void onAcquisitionTypeChanged();

private:
    domain::AcquisitionType selectedAcquisitionType() const;

    Ui::VehicleDetailsView *ui;

    // Etiquetas y campos que se muestran u ocultan según la rama. Se guardan
    // en listas y no como miembros sueltos porque hay que esconder también
    // las etiquetas: dejar una etiqueta huérfana junto a un campo invisible
    // es peor que no ocultar nada. Se arman en el constructor con los widgets
    // del .ui.
    QList<QWidget *> m_acquisitionOnlyWidgets;
    QList<QWidget *> m_consignmentOnlyWidgets;

    // Todos los subtipos con su tipo padre; reloadSubtypes() filtra.
    QList<application::CatalogOptionDto> m_subtypes;
};

#endif // PRESENTATION_INVENTORY_REGISTRATION_STEPS_DETAILS_VEHICLEDETAILSVIEW_H
