#ifndef STEP1DETAILSVIEW_H
#define STEP1DETAILSVIEW_H

#include "domain/validationresult.h"
#include "domain/vehiclebuilder.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTextEdit;

// Paso 1 del wizard: datos generales del vehículo + contraparte + condiciones
// de la operación. Cubre las dos ramas -- Adquisición (la agencia compra) y
// Consignación (la agencia vende por cuenta del dueño y cobra comisión) --
// mostrando los campos de una u otra según el selector de tipo.
//
// Único paso con campos obligatorios. Cuáles y con qué reglas lo decide el
// dominio, no esta vista: validate() arma un VehicleBuilder y le pregunta.
class Step1DetailsView : public QWidget
{
    Q_OBJECT

public:
    explicit Step1DetailsView(QWidget *parent = nullptr);

    // Llena Tipo Vehículo / Marca desde los catálogos y lee el valor UMA
    // vigente de global_configurations (consulta síncrona, catálogos
    // pequeños -- mismo criterio que Step2ConditionView::loadLookups()).
    void loadLookups();

    // Vuelca los widgets sobre el builder. Se llama en cada intento de
    // guardado: la fuente de verdad son los widgets, no un objeto que haya
    // que mantener sincronizado.
    void applyTo(domain::VehicleBuilder &builder) const;

    // Ya no comprueba las reglas por su cuenta: arma un builder con lo
    // capturado y deja que el dominio decida. Así la regla del pago en
    // efectivo o el mínimo del precio viven en un solo sitio, y esta vista
    // solo traduce el resultado a un mensaje en pantalla.
    domain::ValidationResult validate() const;

    void showError(const QString &message);
    void hideError();

private slots:
    void reloadSubtypes();
    void onBrowseInvoiceFile();
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
    QString m_invoiceFilePath;
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

    QLabel *m_errorLabel;

    double m_umaValue = 108.57;
};

#endif // STEP1DETAILSVIEW_H
