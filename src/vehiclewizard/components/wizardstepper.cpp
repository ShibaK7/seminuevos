#include "../../../include/vehiclewizard/components/wizardstepper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>

namespace {
constexpr int kBadgeIconSize = 26;
constexpr int kTextIconSpacing = 10;

// QPushButton::sizeHint() se calcula a partir de text()/icon(), que aquí
// quedan vacíos (el contenido real vive en un layout hijo con QLabels) --
// sin este override el botón se encoge al tamaño de un botón vacío y
// recorta su propio contenido. Reenvía al sizeHint() del layout instalado.
class StepButton : public QPushButton
{
public:
    using QPushButton::QPushButton;

    QSize sizeHint() const override
    {
        return layout() ? layout()->sizeHint() : QPushButton::sizeHint();
    }
};
} // namespace

WizardStepper::WizardStepper(const QStringList &stepLabels, QWidget *parent)
    : QWidget(parent)
    , m_labels(stepLabels)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);

    const QPixmap badgePixmap(QStringLiteral(":/icons/check-badge-green.png"));

    for (int i = 0; i < m_labels.size(); ++i) {
        auto *button = new StepButton(this);
        button->setProperty("class", QStringLiteral("wizard-step"));
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        connect(button, &QPushButton::clicked, this, [this, i]() { emit stepClicked(i); });

        auto *textLabel = new QLabel(m_labels.at(i), button);
        textLabel->setProperty("class", QStringLiteral("wizard-step-text"));

        auto *iconLabel = new QLabel(button);
        iconLabel->setFixedSize(kBadgeIconSize, kBadgeIconSize);
        iconLabel->setPixmap(badgePixmap.scaled(kBadgeIconSize, kBadgeIconSize,
                                                 Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setVisible(false);

        // Texto + ícono a mano (no QPushButton::setText/setIcon) para poder
        // controlar el espacio entre ambos y mantenerlos centrados
        // verticalmente entre sí sin depender de las métricas internas del
        // estilo nativo del botón.
        auto *innerLayout = new QHBoxLayout(button);
        // El botón ya no usa el "padding" de QSS para su tamaño (ver
        // StepButton::sizeHint) -- este margen es lo que hace que el fondo/
        // subrayado del paso sobresalga unos px más allá del texto/ícono.
        innerLayout->setContentsMargins(10, 8, 10, 8);
        innerLayout->setSpacing(kTextIconSpacing);
        innerLayout->addWidget(textLabel, 0, Qt::AlignVCenter);
        innerLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

        layout->addWidget(button);
        m_buttons << button;
        m_textLabels << textLabel;
        m_iconLabels << iconLabel;
        m_completed << false;
    }
    layout->addStretch();

    refreshButton(0);
}

void WizardStepper::setCurrentStep(int index)
{
    if (index < 0 || index >= m_buttons.size() || index == m_currentStep)
        return;

    const int previous = m_currentStep;
    m_currentStep = index;
    refreshButton(previous);
    refreshButton(m_currentStep);
}

void WizardStepper::setStepCompleted(int index, bool completed)
{
    if (index < 0 || index >= m_buttons.size())
        return;

    m_completed[index] = completed;
    refreshButton(index);
}

void WizardStepper::refreshButton(int index)
{
    QPushButton *button = m_buttons.at(index);
    QLabel *textLabel = m_textLabels.at(index);
    QLabel *iconLabel = m_iconLabels.at(index);
    const bool completed = m_completed.at(index);
    const bool isCurrent = (index == m_currentStep);

    iconLabel->setVisible(completed);

    const QString state = isCurrent ? QStringLiteral("current")
                                     : (completed ? QStringLiteral("done") : QStringLiteral("pending"));
    button->setProperty("stepState", state);
    textLabel->setProperty("stepState", state);

    button->style()->unpolish(button);
    button->style()->polish(button);
    textLabel->style()->unpolish(textLabel);
    textLabel->style()->polish(textLabel);
}
