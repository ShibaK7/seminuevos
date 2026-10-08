#ifndef PRESENTATION_VIEWS_SUPPORT_FORMSUPPORT_H
#define PRESENTATION_VIEWS_SUPPORT_FORMSUPPORT_H

#include <functional>

class QObject;
class QWidget;

// Funciones de apoyo para los formularios de Qt Widgets. Son libres y no
// pertenecen a ninguna pantalla: viven aquí para que cada vista no las
// reescriba a su manera.
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

} // namespace formsupport

#endif // PRESENTATION_VIEWS_SUPPORT_FORMSUPPORT_H
