#ifndef STEP2CONDITIONVIEW_H
#define STEP2CONDITIONVIEW_H

#include "../vehicledraft.h"

#include <QList>
#include <QWidget>

class QComboBox;
class QSpinBox;
class ConditionChecklistRow;

// Paso 2 del wizard: especificaciones básicas (izquierda) + checklist de
// condición agrupado (derecha), clasificado a partir de las capturas legacy
// (ver conditionchecklistitems.h). Sin validación obligatoria -- consistente
// con el post-it del wireframe original ("no hay mínimos definidos").
class Step2ConditionView : public QWidget
{
    Q_OBJECT

public:
    explicit Step2ConditionView(QWidget *parent = nullptr);

    // Consulta fuel_type_cat y llena el combo de Combustible. Se llama una
    // vez al abrir el wizard (consulta síncrona a un catálogo pequeño, igual
    // que hace SchemaInitializer en el arranque de la app).
    void loadLookups();

    void fillDraft(VehicleDraft &draft) const;

private:
    QWidget *buildBasicSpecsPanel();
    QWidget *buildChecklistPanel();
    void markAllInGroup(const QString &group, bool checked);

    QComboBox *m_fuelTypeCombo;
    QComboBox *m_cylindersCombo;
    QComboBox *m_transmissionCombo;
    QComboBox *m_interiorMaterialCombo;
    QComboBox *m_windowRegulatorsCombo;
    QComboBox *m_airConditioningCombo;

    QList<ConditionChecklistRow *> m_checklistRows;
};

#endif // STEP2CONDITIONVIEW_H
