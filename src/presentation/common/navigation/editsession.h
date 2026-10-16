#ifndef PRESENTATION_COMMON_NAVIGATION_EDITSESSION_H
#define PRESENTATION_COMMON_NAVIGATION_EDITSESSION_H

namespace presentation {

// Consulta o edición de un formulario que ya existe, como el expediente de un
// vehículo (#33).
enum class FormMode { View, Edit };

// Lleva el modo de un formulario que se consulta y se corrige, y si tiene
// cambios sin guardar. Es estado de presentación, sin widgets (solo QtCore):
// lo usa el presenter y se prueba sin pantallas.
//
// "Sucio" es una bandera que suben las ediciones (markEdited()), no una
// comparación campo por campo contra lo guardado: una comparación escrita a
// mano falla en silencio el día que alguien agrega un campo y olvida
// compararlo. Solo cuenta en Edit: en consulta nadie edita, y las señales que
// disparan los campos al pintar lo guardado no son cambios del usuario.
class EditSession
{
public:
    FormMode mode() const;
    bool isEditing() const;

    // Si se puede pasar a Edit: lo decide quien la usa (el rol, que lo
    // cargado se pueda corregir). No cambia el modo: si ya se está editando,
    // qué hacer con lo capturado lo decide quien la llama.
    void allowEditing(bool allowed);
    bool canEdit() const;

    // Pasa a Edit, sin cambios. Devuelve si cambió de modo: false si no se
    // puede editar o si ya se estaba editando (y entonces conserva lo que
    // hubiera).
    bool beginEdit();
    // El usuario cambió algo. Fuera de Edit no cuenta.
    void markEdited();
    bool isDirty() const;
    // Vuelve a View, sin cambios: al guardar o al descartarlos.
    void endEdit();

    // Si salir (cerrar, abrir otro folio, cerrar la ventana) perdería algo:
    // solo en Edit con cambios.
    bool needsConfirmationToLeave() const;

private:
    FormMode m_mode = FormMode::View;
    bool m_canEdit = false;
    bool m_dirty = false;
};

} // namespace presentation

#endif // PRESENTATION_COMMON_NAVIGATION_EDITSESSION_H
