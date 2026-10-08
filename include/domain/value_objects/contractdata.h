#ifndef DOMAIN_VALUE_OBJECTS_CONTRACTDATA_H
#define DOMAIN_VALUE_OBJECTS_CONTRACTDATA_H

#include <QMap>
#include <QString>
#include <QStringList>

namespace domain {

// Lo que el contrato de una operación dice, ya decidido por el dominio. El
// adaptador del PDF solo lo dibuja sobre la plantilla: no vuelve a preguntarle
// nada a la unidad.
//
// Es un valor, así que la pantalla lo puede guardar después de registrar para
// imprimir cuando el usuario quiera, sin quedarse con la entidad.
struct ContractData
{
    QString title;
    // Lo que la agencia paga al vendedor en una compra, o el precio base en una
    // consignación (mismo contrato, solo cambia el título).
    double amount = 0.0;
    // Marcas {{clave}} de la plantilla, en texto plano.
    QMap<QString, QString> placeholders;
    // Un renglón por documento que acompaña la operación, en texto plano.
    QStringList documentLines;
    // Sugerencia de nombre de archivo (el VIN).
    QString fileNameHint;
};

} // namespace domain

#endif // DOMAIN_VALUE_OBJECTS_CONTRACTDATA_H
