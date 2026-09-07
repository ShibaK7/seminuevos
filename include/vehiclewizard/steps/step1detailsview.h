#ifndef STEP1DETAILSVIEW_H
#define STEP1DETAILSVIEW_H

#include "domain/validationresult.h"
#include "domain/vehiclebuilder.h"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTextEdit;

// Paso 1 del wizard: datos generales del vehículo + propietario anterior +
// datos de adquisición (solo flujo de Adquisición, ver plan US-03.2). Único
// paso con campos obligatorios (AC2): No. Serie, Marca, Modelo, Precio
// Compra, Propietario -- y la regla UMA para pagos de contado.
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

private:
    QWidget *buildGeneralInfoCard();
    QWidget *buildOwnerAndAcquisitionCard();

    // --- Datos generales ---
    QLabel *m_folioLabel;
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
    QDoubleSpinBox *m_purchasePriceSpin;
    QComboBox *m_paymentTypeCombo;
    QComboBox *m_paymentMethodCombo;
    QDoubleSpinBox *m_maintenanceCostSpin;
    QDoubleSpinBox *m_salePriceSpin;
    QTextEdit *m_observationsEdit;

    QLabel *m_errorLabel;

    double m_umaValue = 108.57;
};

#endif // STEP1DETAILSVIEW_H
