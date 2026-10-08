#include "presentation/views/components/outlinebutton.h"

#include <QHBoxLayout>

namespace {

constexpr int kIconSize = 14;
constexpr int kIconTextSpacing = 8;
// Márgenes internos: son el "padding" real del botón, porque sizeHint() sale
// del layout y no de las métricas del control (ver StatefulTextButton). El
// lateral es generoso a propósito: en una pastilla, el texto pegado a un borde
// curvo se ve más apretado de lo que realmente está.
constexpr int kHorizontalPadding = 26;
constexpr int kVerticalPadding = 6;

// Alto fijo. Se fija aquí, y no se deja que salga del contenido, porque el
// radio de la pastilla vive en el QSS y tiene que ser la MITAD de este número:
// si el alto variaba con la fuente o el icono, el radio acababa siendo mayor
// que la mitad y Qt dejaba de redondear del todo, devolviendo un rectángulo.
//
// *** Al cambiar este valor hay que cambiar el border-radius de
//     QPushButton[class="outline-button"] en global-style-clean.qss a la mitad. ***
constexpr int kHeight = 34;

} // namespace

OutlineButton::OutlineButton(QWidget *parent)
    : StatefulTextButton(QString(), QStringLiteral("outline-button"), parent)
{
    setFixedHeight(kHeight);

    QHBoxLayout *layout = contentLayout();
    layout->setContentsMargins(kHorizontalPadding, kVerticalPadding, kHorizontalPadding,
                               kVerticalPadding);
    layout->setSpacing(kIconTextSpacing);

    // Sin addStretch: el botón se ajusta a su contenido. Con él, el contorno se
    // estiraría por todo el espacio libre de la fila.
}

OutlineButton::OutlineButton(const QString &title, const QString &iconPath, QWidget *parent)
    : OutlineButton(parent)
{
    setTitle(title);
    setLeadingIcon(iconPath);
}

QString OutlineButton::leadingIcon() const
{
    return leadingIconPath();
}

void OutlineButton::setLeadingIcon(const QString &iconPath)
{
    // A diferencia del renglón del menú, aquí no hay hermanos con los que
    // alinear el título: sin icono no se reserva la caja, porque el hueco
    // vacío correría el texto hacia la derecha dentro de la pastilla.
    if (iconPath.isEmpty() && leadingIconPath().isEmpty())
        return;
    StatefulTextButton::setLeadingIcon(iconPath, kIconSize);
}
