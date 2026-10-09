#ifndef PRESENTATION_COMMON_FORMS_FORMSUPPORT_H
#define PRESENTATION_COMMON_FORMS_FORMSUPPORT_H

#include "application/common/dto/catalogoptiondto.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>

#include <functional>

class QObject;
class QWidget;

// Funciones de apoyo para los formularios de Qt Widgets. Son libres y no
// pertenecen a ninguna pantalla: viven aquí para que cada vista no las
// reescriba a su manera.
class QComboBox;
class QLabel;

namespace formsupport {

// Llama a onEdit cada vez que cambia algún campo dentro de root: textos
// (QLineEdit, QTextEdit, QPlainTextEdit), combos (índice y texto editable),
// spinboxes, fechas y botones marcables (casillas, radios).
//
// Solo ve los widgets que existen al llamarla: lo que se cree después -- por
// ejemplo, filas que una vista vuelve a armar -- queda sin vigilar. Por eso se
// llama después de poblar el formulario.
//
// context es el receptor de todas las conexiones: cuando se destruye, Qt las
// corta, y onEdit nunca corre sobre un objeto muerto.
//
// Un mismo cambio puede avisar más de una vez (un QSpinBox y el QLineEdit que
// lleva dentro emiten los dos), así que onEdit tiene que tolerar llamadas
// repetidas. El asistente de registro lo resuelve con un debounce.
void watchEdits(QWidget *root, QObject *context, std::function<void()> onEdit);

// --- Campos con error ---
//
// Cada widget de captura lleva en la propiedad dinámica "field" la clave con la
// que el dominio reporta sus errores ("serialNumber", "counterparty.fullName",
// "conditions.transmission"). Con esa clave, estas funciones encuentran el
// widget de cada error sin que la vista tenga que traducir uno por uno.
//
// Un campo con error se marca con la propiedad dinámica "hasError" y el QSS
// global lo pinta con la regla [hasError="true"]. No se usa setStyleSheet()
// por dos razones:
//   - La hoja propia de un widget le gana a la de la aplicación, sin importar
//     qué tan específica sea la regla: en ese widget dejan de aplicar el
//     foco, el hover y el deshabilitado del QSS, y los colores del error
//     quedan regados por el código en vez de vivir en la hoja.
//   - Cada llamada vuelve a interpretar la hoja y a recalcular el estilo del
//     widget y de todo lo que lleva adentro. Con la validación en vivo, eso
//     pasaría en cada tecla.

// Marca los campos de root que tienen error y limpia los demás. Recorre los
// descendientes de root que llevan "field" (no vacío); un widget tiene error
// si algún error trae exactamente su clave. La comparación es exacta:
// "counterparty.fullName" no marca un campo "fullName".
//
// Solo vuelve a aplicar el QSS a los widgets cuyo hasError cambió: con la
// validación en vivo esto corre cada vez que el usuario deja de teclear, y
// repasar la hoja completa en todos los campos del paso se nota.
//
// El tooltip de un campo con error muestra sus mensajes. El tooltip que el
// campo tenía antes se guarda una sola vez en la propiedad "originalToolTip"
// y vuelve en cuanto el campo deja de tener error.
//
// Una lista vacía limpia todo, igual que clearFieldErrors().
void showFieldErrors(QWidget *root, const QList<domain::ValidationError> &errors);

// Quita las marcas de error de todos los campos de root.
void clearFieldErrors(QWidget *root);

// Le da el foco al primer widget de root con esa clave en "field", con
// Qt::OtherFocusReason. Se salta los que no pueden recibirlo: deshabilitados
// u ocultos dentro de root, como los campos de la otra rama de operación.
//
// Devuelve si encontró a cuál dárselo. Así quien la llama puede probar con el
// siguiente error cuando uno no tiene widget en pantalla (la UMA, por
// ejemplo, que no se captura).
bool focusField(QWidget *root, const QString &field);

// --- Ayudas de construcción (antes en UIUtils, que además corría SQL) ------

// Sombra suave de tarjeta. Designer no la puede declarar en el .ui, así que
// se aplica en el constructor de la vista.
void applyFloatingShadow(QWidget *widget, int xOffset = 0, int yOffset = 5, int blur = 25,
                         int opacity = 25);

// Hoja de estilos de una pantalla, leída de un recurso compilado
// (":/styles/login.qss"). Vacía si el recurso no existe. Se guarda en caché:
// la tarjeta del inventario la pide una vez por unidad.
QString styleSheetResource(const QString &resourcePath);

// Etiqueta con el asterisco rojo de "obligatorio".
QLabel *requiredLabel(const QString &text, QWidget *parent = nullptr);

// Llena un combo con opciones de catálogo (texto = nombre, userData = id) y
// lo deja sin elegir (índice -1) con el placeholder dado.
void fillCombo(QComboBox *combo, const QList<application::CatalogOptionDto> &options,
               const QString &placeholder = QStringLiteral("Seleccione una opción..."));

} // namespace formsupport

#endif // PRESENTATION_COMMON_FORMS_FORMSUPPORT_H
