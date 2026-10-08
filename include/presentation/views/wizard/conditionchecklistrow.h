#ifndef PRESENTATION_VIEWS_WIZARD_CONDITIONCHECKLISTROW_H
#define PRESENTATION_VIEWS_WIZARD_CONDITIONCHECKLISTROW_H

#include "application/dto/catalogdtos.h"
#include "application/dto/registrationdtos.h"

#include <QList>
#include <QWidget>

#include <optional>

class QCheckBox;
class QRadioButton;
class QLineEdit;

// Una fila del checklist del Paso 2: casilla "la unidad trae este elemento" +
// radio Estado óptimo / <etiqueta negativa> + observaciones. Se construye a
// partir de un renglón de vehicle_conditions_cat.
//
// Las dos columnas de etiqueta tienen ancho variable según el ítem, así que
// se miden una vez sobre TODO el catálogo (measureColumns) y el ancho se
// inyecta en cada fila, para que queden alineadas en columna en vez de que
// cada una se ajuste a su propio texto.
class ConditionChecklistRow : public QWidget
{
    Q_OBJECT

public:
    struct ColumnWidths
    {
        int checkBox = 0;
        int faultyRadio = 0;
    };

    // Mide sobre el catálogo completo. Antes esto se cacheaba en una estática
    // dentro del constructor, lo que era correcto mientras la lista era una
    // constante de compilación; con el catálogo viniendo de la base dejó de
    // serlo, porque el asistente se reconstruye cada vez que se abre y el
    // ancho quedaba congelado con los datos de la primera apertura.
    static ColumnWidths measureColumns(const QList<application::ChecklistItemDto> &items);

    ConditionChecklistRow(const application::ChecklistItemDto &item, const ColumnWidths &columns,
                          QWidget *parent = nullptr);

    // Para agrupar y para el botón "Marcar todo", sin tener que construir un
    // valor completo solo para leer a qué grupo pertenece la fila.
    const QString &category() const;

    // nullopt cuando la casilla está desmarcada: la unidad no trae ese
    // elemento y por lo tanto NO le corresponde un renglón en
    // vehicle_inspection. Devolver un optional y no un InspectionItem con una
    // bandera adentro es lo que mantiene al dominio sin poder representar un
    // "ítem ausente", que es justo el estado que se quiso eliminar.
    std::optional<application::InspectionEntryDto> value() const;
    void setChecked(bool checked);
    bool isChecked() const;

private slots:
    void updateRowState();

private:
    application::ChecklistItemDto m_item;
    QCheckBox *m_checkBox;
    QRadioButton *m_optimalRadio;
    QRadioButton *m_faultyRadio;
    QLineEdit *m_observationsEdit;
};

#endif // PRESENTATION_VIEWS_WIZARD_CONDITIONCHECKLISTROW_H
