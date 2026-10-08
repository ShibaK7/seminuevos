#ifndef PRESENTATION_VIEWS_COMPONENTS_SIDEBARMENUITEM_H
#define PRESENTATION_VIEWS_COMPONENTS_SIDEBARMENUITEM_H

#include "presentation/views/components/statefultextbutton.h"

#include <QString>

// Un renglón del menú lateral: icono guía a la izquierda y título a su
// derecha, dentro de una pastilla redondeada que se pinta de gris cuando el
// renglón está seleccionado. Es el mismo componente para los cuatro módulos
// (Inventario, Comercial, Finanzas y Reportes): lo único que cambia entre
// ellos es su título y su icono.
//
// De StatefulTextButton hereda el título como etiqueta hija, el sizeHint
// tomado del layout y el manejo de estados -- ver esa clase para el porqué de
// cada uno. Lo propio de este componente es el icono guía y el espaciado.
//
// Deriva de QPushButton (vía la base) en vez de envolver uno, y eso le da
// gratis lo que el menú ya necesitaba: checkable + autoExclusive hacen que
// los cuatro renglones se excluyan entre sí sin que nadie los coordine, y
// clicked() sigue siendo la señal que MainWindow conecta.
//
// Se promueve en Qt Designer (clase base QPushButton): el título y el icono
// llegan como propiedades dinámicas `title` y `leadingIcon` del .ui. La clase
// de QSS, checkable y autoExclusive los fija el constructor, así que no se
// marcan allá.
class SidebarMenuItem : public StatefulTextButton
{
    Q_OBJECT
    // Ruta de recurso Qt (":/icons/car.png"). Se guarda la ruta y no un QIcon
    // ya armado porque el icono se pinta como pixmap escalado en un QLabel, no
    // con el mecanismo de iconos del botón.
    Q_PROPERTY(QString leadingIcon READ leadingIcon WRITE setLeadingIcon)

public:
    // El que usa uic con el widget promovido: arranca sin título y con la caja
    // del icono vacía, y el .ui completa los dos por sus propiedades.
    explicit SidebarMenuItem(QWidget *parent = nullptr);
    SidebarMenuItem(const QString &title, const QString &iconPath, QWidget *parent = nullptr);

    QString leadingIcon() const;
    // Fija el tamaño que le corresponde al icono en la barra y delega en la
    // base, que es donde vive el escalado.
    void setLeadingIcon(const QString &iconPath);
};

#endif // PRESENTATION_VIEWS_COMPONENTS_SIDEBARMENUITEM_H
