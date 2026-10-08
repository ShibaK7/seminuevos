#ifndef PRESENTATION_VIEWS_COMPONENTS_NAVTABITEM_H
#define PRESENTATION_VIEWS_COMPONENTS_NAVTABITEM_H

#include "presentation/views/components/statefultextbutton.h"

#include <QString>

class QFrame;

// Una pestaña de la fila de navegación del contenido (Adquisición /
// Consignación). Solo texto y un subrayado: sin fondo, sin borde y sin icono.
//
// El subrayado es un WIDGET, no el `border-bottom` del botón. Con el borde de
// QSS la selección se notaba en el texto pero la línea no llegaba a
// dibujarse: Qt no aplica de forma fiable un borde sobre un QPushButton sin
// fondo. Un QFrame de 2 px con background-color se pinta siempre, y además
// deja la altura del subrayado bajo control explícito.
//
// La línea gris fina que recorre todo el ancho NO sale de aquí: la pinta el
// contenedor (QFrame[class="navBar"]), para que continúe más allá de la
// última pestaña y haga de divisor con el contenido de abajo. El subrayado de
// la pestaña activa queda justo encima de ella.
//
// Comparte tipografía y colores con SidebarMenuItem por herencia de
// StatefulTextButton: son dos controles de navegación y no tendría por qué
// leerse distinto el texto de uno y de otro.
class NavTabItem : public StatefulTextButton
{
    Q_OBJECT

public:
    // Grosor del subrayado. Público porque el divisor gris de la pantalla se
    // posiciona DENTRO de esta franja para quedar tapado por ella, y necesita
    // el mismo número: duplicarlo a mano dejaría los dos desalineados en
    // cuanto alguien cambiara uno.
    static constexpr int kUnderlineHeight = 2;

    explicit NavTabItem(const QString &title, QWidget *parent = nullptr);

protected:
    // Amplía la base para teñir también el subrayado.
    void applyState(const QString &state) override;

private:
    QFrame *m_underline;
};

#endif // PRESENTATION_VIEWS_COMPONENTS_NAVTABITEM_H
