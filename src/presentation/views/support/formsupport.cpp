#include "presentation/views/support/formsupport.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QWidget>

namespace formsupport {

void watchEdits(QWidget *root, QObject *context, std::function<void()> onEdit)
{
    if (!root || !context || !onEdit)
        return;

    // Cada connect() guarda su propia copia de onEdit, así que no importa que
    // el parámetro deje de existir al salir de aquí.
    for (QLineEdit *edit : root->findChildren<QLineEdit *>())
        QObject::connect(edit, &QLineEdit::textChanged, context, onEdit);
    for (QTextEdit *edit : root->findChildren<QTextEdit *>())
        QObject::connect(edit, &QTextEdit::textChanged, context, onEdit);
    for (QPlainTextEdit *edit : root->findChildren<QPlainTextEdit *>())
        QObject::connect(edit, &QPlainTextEdit::textChanged, context, onEdit);

    for (QComboBox *combo : root->findChildren<QComboBox *>()) {
        QObject::connect(combo, &QComboBox::currentIndexChanged, context, onEdit);
        // Un combo editable (el de la marca, por ejemplo) cambia de texto
        // mientras se escribe sin cambiar de índice.
        QObject::connect(combo, &QComboBox::editTextChanged, context, onEdit);
    }

    for (QSpinBox *spin : root->findChildren<QSpinBox *>())
        QObject::connect(spin, &QSpinBox::valueChanged, context, onEdit);
    for (QDoubleSpinBox *spin : root->findChildren<QDoubleSpinBox *>())
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, context, onEdit);
    for (QDateEdit *edit : root->findChildren<QDateEdit *>())
        QObject::connect(edit, &QDateEdit::dateChanged, context, onEdit);

    // Solo los marcables: un botón común, como "Marcar todo", no es un dato
    // del formulario. Lo que sí avisa son las casillas que ese botón cambia.
    for (QAbstractButton *button : root->findChildren<QAbstractButton *>()) {
        if (button->isCheckable())
            QObject::connect(button, &QAbstractButton::toggled, context, onEdit);
    }
}

} // namespace formsupport
