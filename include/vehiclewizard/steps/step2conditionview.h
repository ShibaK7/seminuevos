#ifndef STEP2CONDITIONVIEW_H
#define STEP2CONDITIONVIEW_H

#include "../../db/conditioncatalog.h"
#include "domain/validationresult.h"
#include "domain/vehiclebuilder.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;
class QVBoxLayout;
class QSqlDatabase;
class ConditionChecklistRow;

// Paso 2 del wizard: especificaciones básicas (izquierda) + checklist de
// condición agrupado (derecha). Los renglones del checklist se leen de
// vehicle_conditions_cat, así que agregar o renombrar un elemento no
// requiere recompilar.
//
// Las especificaciones básicas son obligatorias (combustible, cilindros,
// transmisión, interiores, cristales y aire acondicionado), pero cuáles y con
// qué reglas
// lo decide el dominio, no esta vista: validate() arma un VehicleBuilder y le
// pregunta, igual que el Paso 1. El checklist, en cambio, no tiene mínimos --
// consistente con el post-it del wireframe original ("no hay mínimos
// definidos").
class Step2ConditionView : public QWidget
{
    Q_OBJECT

public:
    explicit Step2ConditionView(QWidget *parent = nullptr);

    // Llena el combo de combustible y construye el checklist desde la base.
    // Se llama una vez al abrir el wizard. El constructor deja el panel
    // armado pero vacío: sin conexión garantizada no se puede poblar.
    void loadLookups();

    // Vuelca los widgets sobre el builder. Un combo sin elegir no se vuelca,
    // para que el dominio lo reporte como faltante en vez de recibir un valor
    // que nadie capturó.
    void applyTo(domain::VehicleBuilder &builder) const;

    // Arma un builder desechable con lo capturado y le pregunta al dominio
    // solo por lo de este paso (validateConditionData()), sin reclamar datos
    // que se capturan en los otros.
    domain::ValidationResult validate() const;

private:
    QWidget *buildBasicSpecsPanel();
    QWidget *buildChecklistPanel();
    void populateChecklist(const QList<ConditionCatalogItem> &items);
    // Mensaje visible en el panel del checklist. Sin esto, un fallo de la
    // consulta dejaba media pantalla en blanco sin explicación.
    void showChecklistMessage(const QString &message);
    void markAllInGroup(const QString &category, bool checked);

    QComboBox *m_fuelTypeCombo;
    QComboBox *m_cylindersCombo;
    QComboBox *m_transmissionCombo;
    QComboBox *m_interiorMaterialCombo;
    QComboBox *m_windowRegulatorsCombo;
    QComboBox *m_airConditioningCombo;

    QWidget *m_checklistContent = nullptr;
    QVBoxLayout *m_checklistLayout = nullptr;
    QLabel *m_checklistMessageLabel = nullptr;
    QList<ConditionChecklistRow *> m_checklistRows;
};

#endif // STEP2CONDITIONVIEW_H
