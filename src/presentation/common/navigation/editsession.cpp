#include "presentation/common/navigation/editsession.h"

namespace presentation {

FormMode EditSession::mode() const
{
    return m_mode;
}

bool EditSession::isEditing() const
{
    return m_mode == FormMode::Edit;
}

void EditSession::allowEditing(bool allowed)
{
    m_canEdit = allowed;
}

bool EditSession::canEdit() const
{
    return m_canEdit;
}

bool EditSession::beginEdit()
{
    if (!m_canEdit || isEditing())
        return false;
    m_mode = FormMode::Edit;
    m_dirty = false;
    return true;
}

void EditSession::markEdited()
{
    if (isEditing())
        m_dirty = true;
}

bool EditSession::isDirty() const
{
    return m_dirty;
}

void EditSession::endEdit()
{
    m_mode = FormMode::View;
    m_dirty = false;
}

bool EditSession::needsConfirmationToLeave() const
{
    return isEditing() && m_dirty;
}

} // namespace presentation
