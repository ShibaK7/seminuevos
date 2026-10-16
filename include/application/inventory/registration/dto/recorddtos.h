#ifndef APPLICATION_INVENTORY_REGISTRATION_DTO_RECORDDTOS_H
#define APPLICATION_INVENTORY_REGISTRATION_DTO_RECORDDTOS_H

#include "application/inventory/registration/dto/registrationdtos.h"
#include "domain/inventory/value_objects/vehicledocument.h"
#include "domain/inventory/value_objects/vehicleimage.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace application {

// DTOs del expediente de un vehículo: consultarlo y corregirlo (US-03.3 y
// US-03.4). Igual que los del alta, son structs planos.

// Qué deja ver y hacer el rol de quien entró. Lo decide VehicleRecordAccess
// en el dominio; la pantalla solo recibe los booleanos, porque la lógica del
// front no puede incluir las reglas del dominio.
struct RecordAccessDto
{
    bool canEdit = false;
    bool showsDealTerms = false;
    bool showsMaintenanceHistory = false;
};

// Una foto guardada, con su miniatura ya leída: la pantalla la pinta sin abrir
// el archivo.
struct StoredImageDto
{
    domain::VehicleImage image;
    QByteArray thumbnail;
};

// El expediente de una unidad tal como se guardó. Compone los DTO de cada
// paso del alta: el expediente ES ese formulario, abierto para un folio que ya
// existe.
//
// Las rutas de archivos (fotos, documentos y factura) llegan del lector
// relativas al almacén, como las guarda la base; el servicio las entrega
// absolutas, listas para abrirse.
struct VehicleRecordDto
{
    int folio = -1;
    // La versión del renglón de la unidad al leerla. Al guardar se compara
    // con la de la base: si otro usuario la cambió entre tanto, el guardado
    // se rechaza en vez de pisar sus cambios.
    QString version;
    // Si existe el renglón de su rama (compra o consignación). Sin él la
    // unidad se puede consultar, pero no corregir: no hay operación que
    // editar.
    bool hasDealTerms = false;
    VehicleDetailsDto details;
    VehicleConditionsDto conditions;
    QList<StoredImageDto> images;
    QList<domain::VehicleDocument> documents;
};

struct RecordLoadResult
{
    enum class Status {
        Loaded,   // record trae el expediente
        NotFound, // ese folio ya no existe
        Failed,   // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    VehicleRecordDto record;
    QString errorMessage; // en Failed
};

// Lo corregido en el expediente, para guardarlo (US-03.4).
struct VehicleUpdateDto
{
    int folio = -1;
    // La versión que se leyó (VehicleRecordDto::version).
    QString version;
    VehicleDetailsDto details;
    VehicleConditionsDto conditions;
    // Rutas absolutas: las de los archivos que ya estaban guardados, tal como
    // las entregó la lectura, y las de los nuevos, donde el usuario los eligió.
    VehicleFilesDto files;
};

struct RecordUpdateResult
{
    enum class Status {
        Saved,    // quedó guardado
        Rejected, // los datos no pasan las reglas, o el VIN o la identificación son de otro
        Conflict, // otro usuario la cambió desde que se leyó; no se guardó nada
        NotFound, // ya no existe
        Failed,   // falló el disco o la base; se puede reintentar
    };
    Status status = Status::Failed;
    domain::ValidationResult validation; // en Rejected, qué campo y por qué
    QString errorMessage;                // en Failed
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_DTO_RECORDDTOS_H
