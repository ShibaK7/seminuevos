#include "presentation/views/components/navtabitem.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {

// El subrayado ocupa todo el ancho del botón, así que estos márgenes laterales
// son lo que lo hace sobresalir un poco del texto en vez de terminar justo en
// la última letra.
constexpr int kHorizontalPadding = 4;
constexpr int kTopPadding = 12;
// Separación entre el texto y el subrayado.
constexpr int kBottomPadding = 10;

} // namespace

NavTabItem::NavTabItem(const QString &title, QWidget *parent)
    : StatefulTextButton(title, QStringLiteral("nav-tab"), parent)
    , m_underline(new QFrame(this))
{
    setCheckable(true);
    // Las pestañas de una misma fila son hermanas, así que esto les basta para
    // excluirse entre sí sin que nadie lleve la cuenta.
    setAutoExclusive(true);

    // Sin addStretch en la fila de contenido: a diferencia del renglón de la
    // barra lateral, la pestaña debe ajustarse a su texto. Si se estirara, el
    // subrayado se extendería por todo el espacio libre de la fila.
    contentLayout()->setContentsMargins(kHorizontalPadding, kTopPadding, kHorizontalPadding,
                                        kBottomPadding);

    m_underline->setProperty("class", QStringLiteral("nav-tab-underline"));
    m_underline->setFixedHeight(kUnderlineHeight);

    m_underline->setAttribute(Qt::WA_TransparentForMouseEvents);
    // Al final del layout raíz, que no tiene márgenes: el subrayado queda
    // pegado al borde inferior del botón y, por lo tanto, justo encima de la
    // línea gris que pinta el contenedor.
    rootLayout()->addWidget(m_underline);

    // La base ya llamó a refreshState() en su constructor, pero entonces
    // m_underline no existía y el despacho virtual todavía no llegaba a
    // este applyState(). Sin esta segunda llamada, la pestaña que arranca
    // seleccionada aparecería sin subrayado hasta el primer clic.
    refreshState();
}

void NavTabItem::applyState(const QString &state)
{
    StatefulTextButton::applyState(state);
    applyStateTo(m_underline, state);
}
