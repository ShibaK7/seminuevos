#include "presentation/common/components/wizardstepper.h"

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

WizardStepper::WizardStepper(QWidget *parent)
    : QWidget(parent)
    , m_row(new QHBoxLayout(this))
{
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(24);
    // Mantiene los pasos juntos a la izquierda; setSteps() lo vuelve a poner
    // al final de la fila cada vez que la reconstruye.
    m_row->addStretch();
}

WizardStepper::WizardStepper(const QStringList &stepLabels, QWidget *parent)
    : WizardStepper(parent)
{
    setSteps(stepLabels);
}

QStringList WizardStepper::steps() const
{
    return m_labels;
}

void WizardStepper::setSteps(const QStringList &stepLabels)
{
    // Fuera la fila anterior, con su stretch final. deleteLater() y no delete:
    // si los pasos se cambian desde un manejador de stepClicked, el botón que
    // emitió el clic todavía está a media ejecución.
    while (QLayoutItem *item = m_row->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_buttons.clear();
    m_textLabels.clear();
    m_iconLabels.clear();
    m_completed.clear();
    m_labels = stepLabels;

    const QPixmap badgePixmap(QStringLiteral(":/icons/check-badge-green.png"));

    for (int i = 0; i < m_labels.size(); ++i) {
        auto *button = new StepButton(this);
        button->setProperty("class", QStringLiteral("wizard-step"));
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

        m_row->addWidget(button);
        m_buttons << button;
        m_textLabels << textLabel;
        m_iconLabels << iconLabel;
        m_completed << false;
    }
    m_row->addStretch();

    // Aspecto de arranque mientras nadie diga otra cosa: el primer paso como
    // actual y los demás pendientes. El wizard lo reemplaza en cuanto valida
    // sus pasos.
    for (int i = 0; i < m_buttons.size(); ++i) {
        setStepState(i, i == 0 ? QStringLiteral("current") : QStringLiteral("pending"), false,
                     QString());
    }
}

void WizardStepper::setStepState(int index, const QString &state, bool complete, const QString &hint)
{
    if (index < 0 || index >= m_buttons.size())
        return;

    QPushButton *button = m_buttons.at(index);
    QLabel *textLabel = m_textLabels.at(index);

    m_completed[index] = complete;
    m_iconLabels.at(index)->setVisible(complete);

    // El cursor y el tooltip van en el botón: los QLabel de adentro no tienen
    // los suyos, así que heredan el cursor y le pasan al botón el evento del
    // tooltip. Los dos aparecen igual al pasar sobre el texto.
    const bool locked = (state == QStringLiteral("locked"));
    button->setCursor(locked ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
    button->setToolTip(locked ? hint : QString());

    // El wizard vuelve a pintar todos los pasos después de cada edición, y casi
    // siempre con el mismo estado. Lo único caro aquí es volver a aplicar el
    // QSS, así que solo se hace cuando el estado cambió de verdad.
    if (button->property("stepState").toString() == state)
        return;

    button->setProperty("stepState", state);
    textLabel->setProperty("stepState", state);

    // Qt no vuelve a aplicar el QSS cuando cambia una propiedad dinámica: hay
    // que despulir y pulir otra vez para que se reevalúe [stepState="..."].
    button->style()->unpolish(button);
    button->style()->polish(button);
    textLabel->style()->unpolish(textLabel);
    textLabel->style()->polish(textLabel);
}

bool WizardStepper::isStepCompleted(int index) const
{
    return index >= 0 && index < m_completed.size() && m_completed.at(index);
}
