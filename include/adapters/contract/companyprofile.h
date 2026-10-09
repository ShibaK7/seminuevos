#ifndef ADAPTERS_CONTRACT_COMPANYPROFILE_H
#define ADAPTERS_CONTRACT_COMPANYPROFILE_H

#include <QString>

// Datos fijos de la concesionaria como COMPRADOR en el contrato de
// compra-venta (siempre es la misma empresa comprando el vehículo al
// propietario anterior). No hay tabla de perfil de empresa en el schema
// porque este dato no cambia por vehículo; si más adelante se necesita
// editar desde la UI, esto se puede mover a global_configurations o a una
// tabla dedicada sin afectar a ContractPdfGenerator.
namespace CompanyProfile
{
inline QString name()
{
    return QStringLiteral("INMOBILIARIA Y DESARROLLO INDEPENDENCIA, S.A DE C.V.");
}

inline QString address()
{
    return QStringLiteral(
        "CARRETERA NACIONAL MANZANA 3 LOTE 10, COL. EL MIRADOR, "
        "MARTINEZ DE LA TORRE, VERACRUZ, C.P. 93607");
}

inline QString identification()
{
    return QString();
}
} // namespace CompanyProfile

#endif // ADAPTERS_CONTRACT_COMPANYPROFILE_H
