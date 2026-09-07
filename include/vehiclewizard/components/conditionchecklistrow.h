#ifndef CONDITIONCHECKLISTROW_H
#define CONDITIONCHECKLISTROW_H

#include "conditionchecklistitems.h"
#include "../vehicledraft.h"

#include <QWidget>

class QCheckBox;
class QRadioButton;
class QLineEdit;

// Una fila del checklist del Paso 2: casilla "incluido en la inspección" +
// radio Estado óptimo / <negativeStateLabel> + observaciones. Reutilizable
// para los ~34 ítems de conditionChecklistItems() -- ver Step2ConditionView.
class ConditionChecklistRow : public QWidget
{
    Q_OBJECT

public:
    explicit ConditionChecklistRow(const ConditionChecklistItemDef &def, QWidget *parent = nullptr);

    ConditionItemValue value() const;
    void setChecked(bool checked);

private slots:
    void updateRowState();

private:
    ConditionChecklistItemDef m_def;
    QCheckBox *m_checkBox;
    QRadioButton *m_optimalRadio;
    QRadioButton *m_faultyRadio;
    QLineEdit *m_observationsEdit;
};

#endif // CONDITIONCHECKLISTROW_H
