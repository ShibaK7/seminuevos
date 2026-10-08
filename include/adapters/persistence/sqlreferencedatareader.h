#ifndef ADAPTERS_PERSISTENCE_SQLREFERENCEDATAREADER_H
#define ADAPTERS_PERSISTENCE_SQLREFERENCEDATAREADER_H

#include "application/ports/referencedatareader.h"

class ConnectionPool;

// Adaptador del puerto ReferenceDataReader sobre los catálogos de PostgreSQL y
// global_configurations. Concentra el SQL que antes corrían las vistas
// (UIUtils::populateComboBox, ConditionCatalog y la lectura de la UMA).
//
// Cada consulta es fija: no se arma SQL con nombres de tabla que lleguen de
// afuera, como hacía populateComboBox.
class SqlReferenceDataReader final : public application::ReferenceDataReader
{
public:
    explicit SqlReferenceDataReader(ConnectionPool &pool);

    QList<application::CatalogOptionDto> vehicleCategories(QString *error) override;
    QList<application::CatalogOptionDto> brands(QString *error) override;
    QList<application::CatalogOptionDto> fuelTypes(QString *error) override;
    QList<application::ChecklistItemDto> conditionChecklist(QString *error) override;
    std::optional<double> umaDailyValue(QString *error) override;

private:
    QList<application::CatalogOptionDto> readOptions(const QString &sql, QString *error);

    ConnectionPool &m_pool;
};

#endif // ADAPTERS_PERSISTENCE_SQLREFERENCEDATAREADER_H
