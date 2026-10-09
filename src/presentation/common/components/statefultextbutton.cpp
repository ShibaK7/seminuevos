#include "presentation/common/components/statefultextbutton.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>

StatefulTextButton::StatefulTextButton(const QString &title, const QString &styleClass,
                                       QWidget *parent)
    : QPushButton(parent)
    , m_rootLayout(new QVBoxLayout(this))
    , m_contentLayout(new QHBoxLayout)
    , m_titleLabel(new QLabel(title, this))
    , m_styleClass(styleClass)
{
    setProperty("class", styleClass);
    setCursor(Qt::PointingHandCursor);
    // El botón no usa setText(), así que sin esto un lector de pantalla
    // anunciaría un botón sin nombre.
    setAccessibleName(title);

    m_titleLabel->setProperty("class", styleClass + QStringLiteral("-title"));
    // Sin esto, el cursor sobre la etiqueta puede dejar al botón sin su
    // pseudo-estado :hover, que es de donde sale el fondo del estado activo:
    // el control se "apagaría" justo al pasar por encima de su propio texto.
    // Con la etiqueta transparente al ratón, el botón es el único que recibe
    // eventos y el estado abarca toda su superficie.
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // El raíz no lleva márgenes ni separación propios: el espaciado del
    // control lo fija cada subclase en la fila de contenido, y lo que se
    // apile debajo debe quedar pegado al borde inferior.
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    m_contentLayout->addWidget(m_titleLabel, 0, Qt::AlignVCenter);
    m_rootLayout->addLayout(m_contentLayout);

    connect(this, &QPushButton::toggled, this, &StatefulTextButton::refreshState);
    refreshState();
}

QString StatefulTextButton::title() const
{
    return m_titleLabel->text();
}

void StatefulTextButton::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
    setAccessibleName(title);
    // El ancho del control sale del layout, y el layout del texto: sin esto,
    // renombrar una pestaña la deja con el ancho del nombre anterior.
    updateGeometry();
}

QHBoxLayout *StatefulTextButton::contentLayout() const
{
    return m_contentLayout;
}

void StatefulTextButton::setLeadingIcon(const QString &iconPath, int size)
{
    m_leadingIconPath = iconPath;

    if (!m_iconLabel) {
        m_iconLabel = new QLabel(this);
        m_iconLabel->setProperty("class", m_styleClass + QStringLiteral("-icon"));
        // Por el mismo motivo que la etiqueta del título: que el estado del
        // control no se apague al pasar el cursor sobre el icono.
        m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        // En la posición 0 está el título, así que insertar aquí es lo que
        // deja el icono DELANTE (leading) y no detrás.
        m_contentLayout->insertWidget(0, m_iconLabel, 0, Qt::AlignVCenter);
    }

    m_iconLabel->setFixedSize(size, size);

    const QPixmap source(iconPath);
    // Ruta vacía, equivocada, o un recurso que no entró al .qrc: la caja queda
    // vacía pero del mismo tamaño, así que los títulos de varios controles
    // hermanos siguen alineados en vez de correrse uno.
    if (source.isNull()) {
        m_iconLabel->clear();
        return;
    }
    m_iconLabel->setPixmap(source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QString StatefulTextButton::leadingIconPath() const
{
    return m_leadingIconPath;
}

QVBoxLayout *StatefulTextButton::rootLayout() const
{
    return m_rootLayout;
}

QSize StatefulTextButton::sizeHint() const
{
    return m_rootLayout->sizeHint();
}

bool StatefulTextButton::event(QEvent *event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        m_hovered = (event->type() == QEvent::Enter);
        refreshState();
    }
    return QPushButton::event(event);
}

void StatefulTextButton::applyStateTo(QWidget *widget, const QString &state)
{
    widget->setProperty("itemState", state);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void StatefulTextButton::applyState(const QString &state)
{
    applyStateTo(m_titleLabel, state);
}

void StatefulTextButton::refreshState()
{
    // "selected" gana sobre "hover": el control activo no cambia de color al
    // pasarle el cursor, porque ya está donde el usuario está.
    const QString state = isChecked() ? QStringLiteral("selected")
                                      : (m_hovered ? QStringLiteral("hover")
                                                   : QStringLiteral("normal"));
    applyState(state);
}
