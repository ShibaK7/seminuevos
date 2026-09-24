#ifndef STATEFULTEXTBUTTON_H
#define STATEFULTEXTBUTTON_H

#include <QPushButton>
#include <QString>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

// Base de los controles de navegación de la aplicación: el renglón de la
// barra lateral (SidebarMenuItem) y la pestaña de contenido (NavTabItem).
//
// Existe porque los dos comparten los mismos tres problemas, ninguno de ellos
// obvio, y resolverlos dos veces era garantía de que se resolvieran distinto:
//
//  1. El texto vive en un QLabel hijo y NO en QPushButton::setText(). Qt no
//     permite fijar la separación entre el icono y el texto de un botón (la
//     toma de una métrica interna del estilo nativo) ni alinearlos de forma
//     explícita. WizardStepper ya había tenido que recurrir a lo mismo.
//  2. Como text() queda vacío, QPushButton::sizeHint() devuelve el tamaño de
//     un botón vacío y el control recorta su propio contenido. Se reenvía al
//     sizeHint del layout. Corolario: el `padding` del QSS deja de influir en
//     el tamaño, y el espaciado real pasa a ser el del layout.
//  3. El color del texto no puede salir de un pseudo-estado del botón, porque
//     Qt no aplica de forma fiable un selector como
//     `QPushButton:checked QLabel` sobre los descendientes. El estado se
//     publica como propiedad dinámica `itemState` en cada hijo que dependa de
//     él y se re-poliza a mano.
//
// La estructura es un layout vertical con una fila de contenido arriba. Se
// eligió vertical, y no una sola fila horizontal, para que las subclases
// puedan apilar algo DEBAJO del contenido: es lo que necesita la pestaña para
// su subrayado, que como widget propio se pinta de forma fiable, mientras que
// un `border-bottom` de QSS sobre un QPushButton no se dibuja.
//
// NO decide nada sobre selección: no marca el control como checkable ni como
// autoExclusive. Eso lo hacen las subclases de navegación, porque para un
// botón de acción sería un error -- se quedaría hundido después del clic.
//
// El constructor es protected: la clase no corresponde por sí sola a ningún
// control de la interfaz, solo al comportamiento que sus herederas comparten.
class StatefulTextButton : public QPushButton
{
    Q_OBJECT

public:
    QString title() const;
    void setTitle(const QString &title);

    QSize sizeHint() const override;

protected:
    // styleClass es el valor de la propiedad `class` que lee el QSS. La
    // etiqueta del título recibe el mismo valor con el sufijo "-title", así
    // que cada subclase obtiene su propio par de selectores sin tener que
    // repetir la convención ni inventarse una nueva.
    StatefulTextButton(const QString &title, const QString &styleClass,
                       QWidget *parent = nullptr);

    // Fila de contenido, con el título ya en la posición 0. Las subclases la
    // usan para insertar contenido propio y para fijar márgenes y separación.
    QHBoxLayout *contentLayout() const;

    // Coloca el icono guía a la izquierda del título, escalado a `size`. La
    // etiqueta se crea la primera vez que se llama y recibe la clase de QSS de
    // la subclase con el sufijo "-icon". Una ruta vacía o un recurso que no
    // exista dejan la caja vacía pero del mismo tamaño, de modo que los
    // títulos de varios controles hermanos siguen alineados entre sí.
    //
    // Vive en la base porque dos de las tres subclases lo necesitan y la
    // tercera podría: escribir el escalado del pixmap tres veces era garantía
    // de que se escribiera distinto.
    void setLeadingIcon(const QString &iconPath, int size);

    // Layout raíz, para apilar algo bajo la fila de contenido.
    QVBoxLayout *rootLayout() const;

    // Vuelca el estado sobre los hijos que dependen de él. La base tiñe el
    // título; una subclase con más partes sensibles al estado amplía esto
    // llamando primero a la implementación heredada.
    virtual void applyState(const QString &state);

    // Fija itemState en un hijo y reaplica el QSS. Qt no repinta solo cuando
    // cambia una propiedad dinámica.
    static void applyStateTo(QWidget *widget, const QString &state);

    // OJO: la base ya la llama en su constructor, pero en ese momento las
    // partes de la subclase todavía no existen Y el despacho virtual aún no
    // alcanza el applyState() derivado. Toda subclase que agregue hijos
    // sensibles al estado debe volver a llamarla al final de su constructor.
    void refreshState();

    // Enter/Leave se atienden por event() y no sobrescribiendo enterEvent():
    // su firma cambió entre Qt 5 y Qt 6 (QEvent* -> QEnterEvent*) y
    // CMakeLists todavía declara compatibilidad con las dos versiones.
    bool event(QEvent *event) override;

private:
    QVBoxLayout *m_rootLayout;
    QHBoxLayout *m_contentLayout;
    QLabel *m_titleLabel;
    // Nulo mientras nadie pida icono: un control sin icono no paga por una
    // etiqueta vacía que además ocuparía espacio en la fila.
    QLabel *m_iconLabel = nullptr;
    QString m_styleClass;
    bool m_hovered = false;
};

#endif // STATEFULTEXTBUTTON_H
