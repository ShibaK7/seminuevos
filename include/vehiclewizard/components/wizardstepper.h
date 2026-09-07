#ifndef WIZARDSTEPPER_H
#define WIZARDSTEPPER_H

#include <QList>
#include <QStringList>
#include <QWidget>

class QPushButton;
class QLabel;

// Fila de navegación superior del wizard (Detalles / Condición / Archivos),
// con marca de check en los pasos completados y subrayado en el paso
// activo -- ver global-style.qss, sección "8. VEHICLE WIZARD"
// (QPushButton[class="wizard-step"] / QLabel[class="wizard-step-text"]).
// El texto y el ícono de la palomita se arman a mano como QLabel hijos del
// botón (en vez de QPushButton::setText/setIcon) para controlar el espacio
// entre ambos y su alineación vertical -- ver refreshButton(). No decide
// por sí mismo si un click es válido: solo avisa via stepClicked() y deja
// que VehicleWizardView decida si permite saltar ahí.
class WizardStepper : public QWidget
{
    Q_OBJECT

public:
    explicit WizardStepper(const QStringList &stepLabels, QWidget *parent = nullptr);

    void setCurrentStep(int index);
    void setStepCompleted(int index, bool completed);

signals:
    void stepClicked(int index);

private:
    void refreshButton(int index);

    QStringList m_labels;
    QList<QPushButton *> m_buttons;
    QList<QLabel *> m_textLabels;
    QList<QLabel *> m_iconLabels;
    QList<bool> m_completed;
    int m_currentStep = 0;
};

#endif // WIZARDSTEPPER_H
