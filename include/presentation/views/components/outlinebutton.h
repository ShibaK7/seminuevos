#ifndef PRESENTATION_VIEWS_COMPONENTS_OUTLINEBUTTON_H
#define PRESENTATION_VIEWS_COMPONENTS_OUTLINEBUTTON_H

#include "presentation/views/components/statefultextbutton.h"

#include <QString>

// Botón de acción de contorno: sin relleno, con borde fino y esquinas
// completamente redondeadas (pastilla). Admite un icono guía opcional a la
// izquierda del texto, como el "+" de "Agregar Vehículo".
//
// A diferencia de SidebarMenuItem y NavTabItem, NO es checkable: es una acción
// que se dispara, no un estado que se elige. Por eso la base dejó de marcar
// checkable por su cuenta; si lo hiciera, este botón se quedaría hundido
// después del primer clic.
//
// Sin fondo a propósito: en una pantalla donde las tarjetas ya son superficies
// blancas sobre blanco, un botón relleno era el único bloque sólido y se comía
// la atención por encima del propio inventario. El contorno lo deja legible
// como acción principal sin competir con el contenido.
//
// Se promueve en Qt Designer (clase base QPushButton) con las propiedades
// dinámicas `title` y `leadingIcon`. La clase de QSS y el alto fijo los pone
// el constructor.
class OutlineButton : public StatefulTextButton
{
    Q_OBJECT
    // Ruta de recurso Qt (":/icons/plus.png"). Vacía = sin icono guía.
    Q_PROPERTY(QString leadingIcon READ leadingIcon WRITE setLeadingIcon)

public:
    // El que usa uic con el widget promovido: arranca sin título ni icono, y
    // el .ui completa los dos por sus propiedades.
    explicit OutlineButton(QWidget *parent = nullptr);
    // iconPath vacío = sin icono guía.
    explicit OutlineButton(const QString &title, const QString &iconPath = QString(),
                           QWidget *parent = nullptr);

    QString leadingIcon() const;
    void setLeadingIcon(const QString &iconPath);
};

#endif // PRESENTATION_VIEWS_COMPONENTS_OUTLINEBUTTON_H
