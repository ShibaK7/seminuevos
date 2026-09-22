#ifndef USERDTO_H
#define USERDTO_H

#include <QString>

// Fila de `users` para pantallas administrativas (Rule 2).
// plainPassword solo viaja UI → servicio; el hash nunca entra en el DTO.
struct UserDTO
{
    int id = 0;
    QString username;
    QString plainPassword;
    QString displayName;
    QString role;
};

#endif // USERDTO_H
