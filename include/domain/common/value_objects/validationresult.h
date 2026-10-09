#ifndef DOMAIN_COMMON_VALUE_OBJECTS_VALIDATIONRESULT_H
#define DOMAIN_COMMON_VALUE_OBJECTS_VALIDATIONRESULT_H

#include <QList>
#include <QString>
#include <QStringList>

namespace domain {

// Un problema concreto encontrado al validar. `field` identifica el dato que
// falla ("serialNumber", "counterparty.fullName") para que la vista pueda
// resaltar el widget correspondiente, no solo mostrar un texto suelto.
struct ValidationError
{
    QString field;
    QString message;   // redactado en español, listo para mostrarse
};

// Resultado de validar una entidad. Acumula errores en vez de detenerse en el
// primero, porque quien captura prefiere ver de una vez todo lo que le falta.
//
// Se usa esto y no un bool + QString suelto por dos razones: la vista quiere
// saber QUÉ campo falló (no solo el mensaje), y merge() permite que una
// entidad componga la validación de sus partes sin perder esa información.
class ValidationResult
{
public:
    ValidationResult() = default;

    void addError(QString field, QString message);

    // Incorpora los errores de otro resultado. fieldPrefix se antepone a cada
    // campo para conservar la ruta: al validar un vehículo, los errores de su
    // contraparte llegan como "counterparty.fullName" y no como "fullName"
    // suelto, que sería ambiguo.
    void merge(const ValidationResult &other, const QString &fieldPrefix = QString());

    bool isValid() const;
    const QList<ValidationError> &errors() const;

    // El primer mensaje, o cadena vacía si no hay errores. Es el reemplazo
    // directo del QString de error que hoy manejan las vistas.
    QString firstMessage() const;
    QString joinedMessages(const QString &separator = QStringLiteral("\n")) const;

    // Los campos con error, para resaltarlos en la interfaz.
    QStringList fields() const;

private:
    QList<ValidationError> m_errors;
};

} // namespace domain

#endif // DOMAIN_COMMON_VALUE_OBJECTS_VALIDATIONRESULT_H
