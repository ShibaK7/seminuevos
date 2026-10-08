#ifndef PRESENTATION_VIEWS_WIZARD_VEHICLEDETAILSVIEW_H
#define PRESENTATION_VIEWS_WIZARD_VEHICLEDETAILSVIEW_H

#include "application/dto/catalogdtos.h"
#include "application/dto/registrationdtos.h"
#include "presentation/presenters/ivehicledetailsview.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;

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
class VehicleDetailsView : public QWidget, public presentation::IVehicleDetailsView
{
    Q_OBJECT

public:
    explicit VehicleDetailsView(QWidget *parent = nullptr);

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
    QWidget *buildGeneralInfoCard();
    QWidget *buildOwnerAndAcquisitionCard();
    domain::AcquisitionType selectedAcquisitionType() const;

    // --- Datos generales ---
    QLineEdit *m_folioEdit;
    QDateEdit *m_dateEdit;
    QComboBox *m_vehicleTypeCombo;
    QComboBox *m_subtypeCombo;
    QComboBox *m_brandCombo;
    QLineEdit *m_modelEdit;
    QSpinBox *m_yearModelSpin;
    QLineEdit *m_colorEdit;
    QSpinBox *m_mileageSpin;
    QTextEdit *m_descriptionEdit;
    QLineEdit *m_motorNumberEdit;
    QLineEdit *m_serialNumberEdit;
    QLineEdit *m_repuveEdit;
    QLineEdit *m_platesEdit;
    QLineEdit *m_platesHolderEdit;

    // --- Tipo de operación ---
    QComboBox *m_acquisitionTypeCombo;
    // La contraparte se llama distinto en cada rama (vendedor o propietario),
    // así que la etiqueta se actualiza junto con el resto.
    QLabel *m_counterpartyLabel;

    // --- Propietario ---
    QLineEdit *m_ownerNameEdit;
    QLineEdit *m_ownerIdEdit;
    QLineEdit *m_ownerAddressEdit;
    QLineEdit *m_ownerSuburbEdit;
    QLineEdit *m_ownerLocalityEdit;
    QLineEdit *m_ownerStateEdit;
    QLineEdit *m_ownerPostalCodeEdit;

    // --- Factura / precio ---
    QComboBox *m_invoiceTypeCombo;
    QLabel *m_invoiceFileLabel;
    QPushButton *m_cfdiRequestButton;
    QPushButton *m_invoiceUploadButton;
    QLineEdit *m_invoiceNumberEdit;
    QLineEdit *m_invoiceIssuerEdit;
    QDoubleSpinBox *m_maintenanceCostSpin;
    QTextEdit *m_observationsEdit;

    // --- Solo Adquisición ---
    QDoubleSpinBox *m_purchasePriceSpin;
    QComboBox *m_paymentTypeCombo;
    QComboBox *m_paymentMethodCombo;
    QDoubleSpinBox *m_salePriceSpin;

    // --- Solo Consignación ---
    QDoubleSpinBox *m_basePriceSpin;
    QDoubleSpinBox *m_commissionRateSpin;

    // Etiquetas y campos que se muestran u ocultan según la rama. Se guardan
    // en listas y no como miembros sueltos porque hay que esconder también
    // las etiquetas: dejar una etiqueta huérfana junto a un campo invisible
    // es peor que no ocultar nada.
    QList<QWidget *> m_acquisitionOnlyWidgets;
    QList<QWidget *> m_consignmentOnlyWidgets;

    // Todos los subtipos con su tipo padre; reloadSubtypes() filtra.
    QList<application::CatalogOptionDto> m_subtypes;
};

#endif // PRESENTATION_VIEWS_WIZARD_VEHICLEDETAILSVIEW_H
