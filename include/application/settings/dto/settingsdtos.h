#ifndef APPLICATION_SETTINGS_DTO_SETTINGSDTOS_H
#define APPLICATION_SETTINGS_DTO_SETTINGSDTOS_H

#include "domain/common/value_objects/validationresult.h"
#include "domain/settings/value_objects/systemparameter.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace application {

// DTOs de los parámetros de Configuraciones (US-07.1). Los valores van en
// centésimos, como en domain::SystemParameter.

// Un parámetro como está guardado.
struct SystemParameterDto
{
    domain::SystemParameter parameter = domain::SystemParameter::UmaDailyValue;
    // nullopt si nunca se ha capturado.
    std::optional<qint64> hundredths;
    // Su último cambio ("Actualizado el ..."); inválida si nunca se capturó.
    QDateTime updatedAt;
};

struct SystemParametersResult
{
    enum class Status {
        Loaded, // parameters trae los cinco, en el orden de pantalla
        Denied, // el rol no entra a Configuraciones
        Failed, // falló la base; se puede reintentar
    };
    Status status = Status::Failed;
    QList<SystemParameterDto> parameters;
    QString errorMessage; // en Failed
};

// Lo que hay en un campo al guardar, junto con lo que tenía al leerse: así se
// sabe qué cambió y si otro administrador lo cambió mientras tanto.
struct SystemParameterEditDto
{
    domain::SystemParameter parameter = domain::SystemParameter::UmaDailyValue;
    std::optional<qint64> loaded; // como se leyó (nullopt: sin capturar)
    std::optional<qint64> edited; // como quedó en pantalla (nullopt: vacío)
};

struct SystemParametersUpdateResult
{
    enum class Status {
        Saved,     // updated dice cuáles cambiaron
        NoChanges, // nada cambió; no se tocó la base
        Rejected,  // validation dice qué campos corregir; no se tocó la base
        Denied,    // el rol no entra a Configuraciones
        Conflict,  // otro administrador cambió alguno desde que se leyó; no se guardó nada
        Failed,    // falló la base y no se guardó nada; se puede reintentar
    };
    Status status = Status::Failed;
    QList<domain::SystemParameter> updated;
    domain::ValidationResult validation; // en Rejected
    QString errorMessage;                 // en Failed
};

} // namespace application

#endif // APPLICATION_SETTINGS_DTO_SETTINGSDTOS_H
