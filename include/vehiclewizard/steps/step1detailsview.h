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
class QPushButton;
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
    void generateCfdiRequest();
    // Muestra los campos de la rama elegida y repuebla el combo de factura.
    void onAcquisitionTypeChanged();
    // Lo que dispara un cambio de tipo de factura: primero la regla sobre el
    // archivo ya elegido (apartarlo o recuperarlo) y después los botones. Es
    // un slot aparte porque refreshInvoiceFileControls() también corre al
    // construir la vista y al generar la solicitud, y en ninguno de esos dos
    // casos procede esa regla: al construir no hay archivo, y generar la
    // solicitud descarta la factura apartada en lugar de recuperarla.
    void onInvoiceTypeChanged();
    // Único sitio que decide si se ve el botón de CFDI y si se puede subir la
    // factura. Recalcula los dos a partir del tipo de factura y de
    // m_cfdiRequestGenerated, en vez de que cada evento toque los botones por
    // su cuenta: así ninguna secuencia de cambios los deja desfasados. Solo
    // toca los botones, no el archivo ya elegido ni la etiqueta que lo nombra:
    // qué pasa con ellos al cambiar de tipo o al generar la solicitud lo
    // deciden onInvoiceTypeChanged() (apartarlo o recuperarlo) y
    // generateCfdiRequest() (descartar la factura apartada).
    void refreshInvoiceFileControls();

private:
    QWidget *buildGeneralInfoCard();
    QWidget *buildOwnerAndAcquisitionCard();
    domain::AcquisitionType selectedAcquisitionType() const;
    // Lo comparten refreshInvoiceFileControls(), onInvoiceTypeChanged() y los
    // dos métodos que apartan y recuperan la factura, para que todos lean el
    // combo igual, también en el instante en que onAcquisitionTypeChanged() lo
    // deja vacío para repoblarlo.
    bool isAutofacturaSelected() const;
    // En autofactura la factura se sube después de generar la solicitud de
    // CFDI, y bloquear el botón no alcanza para garantizarlo: con Facturado,
    // el tipo por omisión, la subida está libre, y una factura elegida ahí
    // seguiría adjunta al cambiar a Autofactura sin que la solicitud se
    // hubiera generado nunca. Por eso, si el tipo pasa a Autofactura sin
    // solicitud, el archivo se aparta: deja de estar adjunto, pero queda en
    // m_suspendedInvoiceFilePath por si se sale de Autofactura.
    // Comprueba por su cuenta las tres condiciones -- Autofactura elegida,
    // solicitud sin generar y un archivo adjunto -- y, si falta alguna, no
    // hace nada. Así se puede llamar desde cualquier sitio, sin dar por hecho
    // que quien la llama ya revisó el tipo.
    void suspendInvoiceFileIfCfdiRequestPending();
    // Vuelve a adjuntar la factura apartada en cuanto el tipo deja de ser
    // Autofactura. Ir y volver en el combo es fácil de hacer sin querer (dos
    // clics, o dos giros de la rueda del ratón sobre él), y no debe costar en
    // silencio la factura que ya se había elegido. Igual que la anterior,
    // comprueba por su cuenta sus dos condiciones -- que el tipo ya no sea
    // Autofactura y que haya una factura apartada -- y sin ellas no hace nada.
    void restoreSuspendedInvoiceFile();

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
    // Miembros y no variables locales del armado de la tarjeta: su estado
    // cambia después de construidos, cada vez que cambia el tipo de factura o
    // se genera la solicitud de CFDI.
    QPushButton *m_cfdiRequestButton;
    QPushButton *m_invoiceUploadButton;
    // Se enciende la primera vez que la solicitud de CFDI llega a abrirse y ya
    // no se apaga: ir y volver entre Facturado y Autofactura no obliga a
    // generarla otra vez ni aparta la factura que ya se haya subido. Tampoco
    // hay que reiniciarla a mano entre un vehículo y otro: cada apertura del
    // asistente construye un Step1DetailsView nuevo, y la bandera nace en
    // false con él.
    bool m_cfdiRequestGenerated = false;
    // La factura elegida con Facturado que se apartó al pasar a Autofactura
    // sin la solicitud de CFDI; vacía si no hay ninguna. Mientras está aquí no
    // cuenta como adjunta: m_invoiceFilePath queda vacío, así que applyTo() no
    // la manda al guardado. Se conserva en vez de tirarla para devolverla si
    // se sale de Autofactura; generar la solicitud sí la descarta, porque en
    // autofactura solo vale la factura que se sube después.
    QString m_suspendedInvoiceFilePath;
    QLineEdit *m_invoiceNumberEdit;
    QLineEdit *m_invoiceIssuerEdit;
    QDoubleSpinBox *m_maintenanceCostSpin;
    QTextEdit *m_observationsEdit;

    // --- Solo Adquisición ---
    QDoubleSpinBox *m_purchasePriceSpin;
    QComboBox *m_paymentTypeCombo;
    QComboBox *m_paymentMethodCombo;
    QDoubleSpinBox *m_salePriceSpin;
    QLabel *m_priceErrorLabel;
    QLabel *m_purchasePriceErrorLabel;

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

    void validatePurchaseConditions();
};

#endif // STEP1DETAILSVIEW_H
