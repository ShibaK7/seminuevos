#ifndef APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLEREGISTRATIONSERVICE_H
#define APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLEREGISTRATIONSERVICE_H

#include "application/inventory/registration/dto/catalogdtos.h"
#include "application/inventory/registration/dto/registrationdtos.h"
#include "application/common/dto/uploaddtos.h"
#include "application/inventory/registration/ports/contractgenerator.h"
#include "application/common/ports/filestorage.h"
#include "application/inventory/registration/ports/referencedatareader.h"
#include "application/inventory/registration/ports/vehiclerepository.h"

namespace application {

// Caso de uso "registrar un vehículo". Antes estaba repartido entre la vista
// (armaba el VehicleBuilder), un QThread en db/ (copiaba los archivos) y el
// repositorio (la transacción).
//
// Solo guarda referencias a sus puertos, así que se puede llamar desde el hilo
// de trabajo. Los métodos validate* son puros (sin E/S): la pantalla los llama
// en cada edición para encender o apagar la palomita de cada paso.
class VehicleRegistrationService final
{
public:
    VehicleRegistrationService(VehicleRepository &vehicles, FileStorage &files,
                               ReferenceDataReader &referenceData, ContractGenerator &contracts);

    // Catálogos y UMA para el asistente. Hace E/S: correrlo fuera del hilo de
    // la interfaz.
    RegistrationLookupsDto loadLookups() const;

    domain::ValidationResult validateDetails(const VehicleDetailsDto &details,
                                             std::optional<double> umaDailyValue) const;
    domain::ValidationResult validateConditions(const VehicleConditionsDto &conditions) const;

    // Valida todo, revisa que el VIN no exista, copia los archivos al almacén
    // y guarda la unidad. Si algo falla después de copiar, borra exactamente lo
    // que copió (nunca carpetas enteras). Hace E/S.
    RegistrationResult registerVehicle(const VehicleRegistrationDto &registration) const;

    ContractGenerator::Outcome generateContract(const domain::ContractData &contract,
                                                const QString &outputPath) const;

    // --- Archivos del Paso 3 y de la factura ---

    // Formatos que admite cada entrada, para el diálogo y el texto de ayuda.
    UploadFormatsDto uploadFormats(UploadKind kind) const;
    // Revisa un archivo elegido contra la regla de formatos de su entrada.
    // Abre el archivo (E/S ligera): solo lee su encabezado.
    UploadCheckDto checkUpload(const QString &path, UploadKind kind) const;
    // Los bytes de un archivo elegido, para su miniatura. Vacío si no se pudo leer.
    QByteArray filePreview(const QString &path) const;
    // Escribe en la carpeta temporal el formulario de solicitud de CFDI (de la
    // autofactura), listo para abrirse en el navegador.
    TemporaryFileDto prepareCfdiRequestForm() const;

private:
    VehicleRepository &m_vehicles;
    FileStorage &m_files;
    ReferenceDataReader &m_referenceData;
    ContractGenerator &m_contracts;
};

} // namespace application

#endif // APPLICATION_INVENTORY_REGISTRATION_SERVICES_VEHICLEREGISTRATIONSERVICE_H
