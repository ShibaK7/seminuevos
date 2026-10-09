#include "presentation/common/components/sidebarmenuitem.h"

#include <QHBoxLayout>

namespace {

constexpr int kIconSize = 20;
// Separación entre el icono y el título. Es justamente el valor que un
// QPushButton con setIcon/setText no permite fijar, y el motivo de que la base
// arme el contenido a mano.
constexpr int kIconTextSpacing = 10;
// Márgenes internos. Son el "padding" real del renglón: el del QSS no cuenta,
// porque sizeHint() sale del layout y no de las métricas del botón.
//
// Izquierda y derecha son distintas a propósito: la sangría izquierda es la
// que separa el icono del borde de la pastilla y da el aire que el renglón
// necesita para no verse apretado, mientras que a la derecha no hay nada que
// separar (el título termina antes, y el resto lo ocupa el addStretch).
constexpr int kLeftPadding = 26;
constexpr int kRightPadding = 14;
constexpr int kVerticalPadding = 13;

} // namespace

SidebarMenuItem::SidebarMenuItem(QWidget *parent)
    : StatefulTextButton(QString(), QStringLiteral("menu-bar"), parent)
{
    setCheckable(true);
    // Los cuatro renglones del menú son hermanos, así que esto solo les basta
    // para excluirse entre sí: marcar uno desmarca los demás sin que nadie
    // lleve la cuenta.
    setAutoExclusive(true);

    QHBoxLayout *layout = contentLayout();
    layout->setContentsMargins(kLeftPadding, kVerticalPadding, kRightPadding, kVerticalPadding);
    layout->setSpacing(kIconTextSpacing);

    // La caja del icono se crea desde ya, aunque la ruta llegue después (por
    // la propiedad leadingIcon del .ui): así el renglón tiene su tamaño final
    // desde el principio y un renglón sin icono conserva la sangría del título.
    setLeadingIcon(QString());

    // Empuja icono y título a la izquierda: la pastilla ocupa todo el ancho de
    // la barra, pero su contenido se alinea al inicio.
    layout->addStretch();
}

SidebarMenuItem::SidebarMenuItem(const QString &title, const QString &iconPath, QWidget *parent)
    : SidebarMenuItem(parent)
{
    setTitle(title);
    setLeadingIcon(iconPath);
}

QString SidebarMenuItem::leadingIcon() const
{
    return leadingIconPath();
}

void SidebarMenuItem::setLeadingIcon(const QString &iconPath)
{
    StatefulTextButton::setLeadingIcon(iconPath, kIconSize);
}
