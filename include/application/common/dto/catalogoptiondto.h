#ifndef APPLICATION_COMMON_DTO_CATALOGOPTIONDTO_H
#define APPLICATION_COMMON_DTO_CATALOGOPTIONDTO_H

#include <QString>

namespace application {

// Una opción de un catálogo (marca, tipo, combustible...). parentId sirve para
// los subtipos de vehículo: -1 si la opción no cuelga de otra. Es común a
// todos los módulos: cualquier combo que se llene desde un catálogo la usa.
struct CatalogOptionDto
{
    int id = -1;
    QString name;
    int parentId = -1;
};

} // namespace application

#endif // APPLICATION_COMMON_DTO_CATALOGOPTIONDTO_H
