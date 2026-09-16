#ifndef SIDEBARMENUITEM_H
#define SIDEBARMENUITEM_H

#include "components/statefultextbutton.h"

#include <QString>

// Un renglón del menú lateral: icono guía a la izquierda y título a su
// derecha, dentro de una pastilla redondeada que se pinta de gris cuando el
// renglón está seleccionado. Es el mismo componente para los cuatro módulos
// (Inventario, Comercial, Finanzas y Reportes): lo único que cambia entre
// ellos es lo que recibe por constructor.
//
// De StatefulTextButton hereda el título como etiqueta hija, el sizeHint
// tomado del layout y el manejo de estados -- ver esa clase para el porqué de
// cada uno. Lo propio de este componente es el icono guía y el espaciado.
//
// Deriva de QPushButton (vía la base) en vez de envolver uno, y eso le da
// gratis lo que el menú ya necesitaba: checkable + autoExclusive hacen que
// los cuatro renglones se excluyan entre sí sin que nadie los coordine, y
// clicked() sigue siendo la señal que MainWindow conecta.
class SidebarMenuItem : public StatefulTextButton
{
    Q_OBJECT

public:
    // iconPath es una ruta de recurso Qt (":/icons/car.png"). Se recibe la
    // ruta y no un QIcon ya armado porque el icono se pinta como pixmap
    // escalado en un QLabel, no con el mecanismo de iconos del botón.
    SidebarMenuItem(const QString &title, const QString &iconPath, QWidget *parent = nullptr);

    // Fija el tamaño que le corresponde al icono en la barra y delega en la
    // base, que es donde vive el escalado.
    void setLeadingIcon(const QString &iconPath);
};

#endif // SIDEBARMENUITEM_H
