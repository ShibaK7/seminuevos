#ifndef ADAPTERS_PERSISTENCE_SETTINGS_ACTIVEADMINISTRATOR_H
#define ADAPTERS_PERSISTENCE_SETTINGS_ACTIVEADMINISTRATOR_H

#include <QString>

class QSqlDatabase;

// Si `actorId` sigue siendo Administrador activo, leído dentro de la
// transacción abierta en `db`. La sesión pudo abrirse antes de que le
// quitaran el rol o lo desactivaran: lo que escribe Configuraciones lo
// pregunta aquí antes de escribir.
//
// Toma un candado FOR SHARE sobre su renglón: si otro administrador le está
// quitando el rol, espera a que confirme y decide con lo vigente. Si la
// consulta falla devuelve false y deja la causa en `error`.
bool isActiveAdministrator(QSqlDatabase &db, int actorId, QString *error);

#endif // ADAPTERS_PERSISTENCE_SETTINGS_ACTIVEADMINISTRATOR_H
