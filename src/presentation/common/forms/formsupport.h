#ifndef PRESENTATION_COMMON_FORMS_FORMSUPPORT_H
#define PRESENTATION_COMMON_FORMS_FORMSUPPORT_H

#include "application/common/dto/catalogoptiondto.h"
#include "domain/common/value_objects/catalogref.h"
#include "domain/common/value_objects/validationresult.h"

#include <QList>
#include <QString>
#include <QVariant>

#include <functional>
#include <optional>

class QDate;
class QObject;
class QWidget;

// Funciones de apoyo para los formularios de Qt Widgets. Son libres y no
// pertenecen a ninguna pantalla: viven aquí para que cada vista no las
// reescriba a su manera.
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QSpinBox;

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

// Le pone a cada campo de root que lleva "field" una etiqueta oculta justo
// debajo, donde showFieldErrors() escribe sus mensajes. Se llama una vez, en
// el constructor de la vista y después de setupUi(): así ningún .ui declara
// una etiqueta por campo, y un campo nuevo con "field" recibe la suya solo.
//
// Cómo hace lugar depende del layout que contiene al campo:
//   - QGridLayout: abre un renglón de errores debajo de cada renglón (el
//     renglón r pasa a ser el 2r y sus errores van en el 2r + 1), y la
//     etiqueta ocupa las mismas columnas que su campo;
//   - QFormLayout: inserta una fila debajo, en la columna de los campos;
//   - QVBoxLayout: la inserta justo después del campo.
// Un renglón cuyos widgets están todos ocultos no ocupa lugar ni suma
// espaciado, así que sin errores el formulario se ve igual que en Designer.
// Un campo en otro layout se queda sin etiqueta: sus mensajes van en su
// tooltip.
//
// El QSS pinta las etiquetas con la clase "field-error".
void addFieldErrorLabels(QWidget *root);

// Marca los campos de root que tienen error y limpia los demás. Recorre los
// descendientes de root que llevan "field" (no vacío); un widget tiene error
// si algún error trae exactamente su clave. La comparación es exacta:
// "counterparty.fullName" no marca un campo "fullName".
//
// Solo vuelve a aplicar el QSS a los widgets cuyo hasError cambió: con la
// validación en vivo esto corre cada vez que el usuario deja de teclear, y
// repasar la hoja completa en todos los campos del paso se nota.
//
// Los mensajes de un campo van en su etiqueta de error, uno por renglón, y la
// etiqueta se oculta cuando el campo deja de tener error. Un campo sin
// etiqueta los muestra en su tooltip: el que tenía antes se guarda una sola
// vez en la propiedad "originalToolTip" y vuelve en cuanto deja de tener
// error.
//
// Devuelve los errores que no se ven en ningún campo de root: los que no
// tienen un widget con su clave, o cuyo widget está oculto dentro de root
// (como los de la otra rama de operación). Quien la llama decide dónde
// mostrarlos.
//
// Una lista vacía limpia todo, igual que clearFieldErrors().
QList<domain::ValidationError> showFieldErrors(QWidget *root,
                                               const QList<domain::ValidationError> &errors);

// Quita las marcas de error de todos los campos de root.
void clearFieldErrors(QWidget *root);

// Le da el foco al primer widget de root con esa clave en "field", con
// Qt::OtherFocusReason. Se salta los que no pueden recibirlo: deshabilitados
// u ocultos dentro de root, como los campos de la otra rama de operación. Si
// el campo está dentro de un área con scroll, la desplaza hasta él.
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

// Iguala el ancho de cada columna entre varias rejillas, para que las de un
// formulario partido en secciones (cada una con su propio .ui) se alineen
// como si fueran una sola. Toma, por columna, el mayor ancho preferido de los
// widgets que ocupan una sola columna (también los de una sección oculta, así
// cambiar de sección no mueve las columnas) y lo pone como ancho mínimo en
// todas. Las etiquetas de error de addFieldErrorLabels() no cuentan: su ancho
// depende del mensaje del momento. Hay que llamarla ya pulida la hoja de
// estilos: el ancho de una etiqueta depende de su fuente.
void alignGridColumns(const QList<QGridLayout *> &grids);

// Etiqueta con el asterisco rojo de "obligatorio".
QLabel *requiredLabel(const QString &text, QWidget *parent = nullptr);

// Llena un combo con opciones de catálogo (texto = nombre, userData = id) y
// lo deja sin elegir (índice -1) con el placeholder dado.
void fillCombo(QComboBox *combo, const QList<application::CatalogOptionDto> &options,
               const QString &placeholder = QStringLiteral("Seleccione una opción..."));

// --- Pintar lo guardado ---

// Elige la opción con el id de `ref`, como se pinta un dato guardado. Si el
// id ya no está en el combo (se quitó del catálogo después de guardar) y la
// referencia trae nombre, la agrega con ese nombre: lo guardado se ve tal
// cual en vez de convertirse en "sin elegir". Una referencia inválida deja el
// combo sin elegir.
void selectOption(QComboBox *combo, const domain::CatalogRef &ref);
// Lo mismo por texto, para los combos cuyo texto es el dato (Cilindros,
// Interiores). Un texto vacío deja el combo sin elegir.
void selectText(QComboBox *combo, const QString &text);
// Elige la opción cuyo userData es `data`. Un QVariant inválido, o un dato
// que el combo no ofrece, lo deja sin elegir.
void selectData(QComboBox *combo, const QVariant &data);

// Los combos de valores cerrados llevan su enum en el userData, como int (así
// los llenan las vistas). Sin valor, el combo queda sin elegir.
template <class Enum>
void selectEnum(QComboBox *combo, Enum value)
{
    selectData(combo, QVariant(static_cast<int>(value)));
}
template <class Enum>
void selectEnum(QComboBox *combo, const std::optional<Enum> &value)
{
    selectData(combo, value ? QVariant(static_cast<int>(*value)) : QVariant());
}

// Un texto guardado se pinta desde su principio: setText() deja el cursor al
// final, y un texto más largo que el campo se vería cortado por el inicio.
void showText(QLineEdit *edit, const QString &text);

// Un número o una fecha guardados se pintan tal cual aunque se salgan del
// rango que el campo ofrece al capturar (una unidad de 1975 contra un año
// mínimo de 1980, un costo que el historial ya acumuló por encima del tope):
// el rango se amplía, nunca se recorta el valor, que cambiaría al guardar.
void showValue(QSpinBox *spin, int value);
void showValue(QDoubleSpinBox *spin, double value);
// Una fecha inválida no se pinta: el campo se queda como estaba.
void showDate(QDateEdit *edit, const QDate &date);

// --- Solo lectura ---
//
// Pone en solo lectura (o vuelve editables) todos los editores bajo root,
// tengan o no "field": textos, spinbox y fechas, combos y casillas o radios.
// Los editores internos de un spinbox o de un combo se manejan desde su dueño.
// Los botones normales no se tocan: cada vista oculta los suyos.
//
// En solo lectura los campos se ven como siempre y se pueden seleccionar y
// copiar, pero no cambian: los textos y spinbox usan su readOnly real (y los
// spinbox pierden sus flechas); los combos, casillas y radios, que no tienen
// uno, llevan la propiedad dinámica "readOnly" y un filtro de eventos que se
// traga los clics y las teclas (salvo Tab) y deja pasar la rueda al área con
// scroll. El QSS lee "readOnly" para quitarles el hover y las flechas.
//
// lockedFields son claves "field" que no se pueden cambiar aunque readOnly
// sea false (al editar el expediente, la rama y el costo de mantenimientos):
// esos se deshabilitan, en gris y con un tooltip que lo explica. Solo se
// rehabilitan los que deshabilitó esta función, así que un campo que ya venía
// deshabilitado (Folio) nunca se habilita.
//
// Se puede llamar las veces que sea: lo original (las flechas de un spinbox,
// el tooltip de un campo bloqueado) se guarda solo al cambiar de estado.
void setReadOnly(QWidget *root, bool readOnly, const QStringList &lockedFields = {});

} // namespace formsupport

#endif // PRESENTATION_COMMON_FORMS_FORMSUPPORT_H
