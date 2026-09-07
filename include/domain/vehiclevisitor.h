#ifndef DOMAIN_VEHICLEVISITOR_H
#define DOMAIN_VEHICLEVISITOR_H

namespace domain {

// Solo declaraciones adelantadas: incluir los headers de las derivadas aquí
// crearía un ciclo, porque vehicle.h necesita conocer esta interfaz. Por eso
// accept() se define fuera de línea, en el .cpp de cada derivada.
class AcquiredVehicle;
class ConsignedVehicle;

// Doble despacho sobre la jerarquía de vehículos.
//
// Resuelve un problema concreto del proyecto, no es adorno: hasta ahora el
// tipo de operación estaba escrito a mano dentro del SQL de registro y la
// rama de consignación simplemente no existía en el código, aunque sí en el
// esquema. Nada avisaba de esa ausencia.
//
// Con esta interfaz, agregar una subclase obliga a declarar un visit() más,
// y eso rompe la compilación de todos los visitantes que no lo implementen.
// La exhaustividad deja de depender de que alguien se acuerde: la garantiza
// el compilador. Un dynamic_cast en el repositorio habría funcionado igual
// hoy y habría vuelto a abrir el mismo agujero mañana.
//
// El dominio no sabe qué hacen los visitantes. El que persiste vive en la
// capa de base de datos; el mismo mecanismo sirve para elegir plantilla de
// contrato o para armar la tarjeta de inventario.
class VehicleVisitor
{
public:
    virtual ~VehicleVisitor() = default;

    virtual void visit(const AcquiredVehicle &vehicle) = 0;
    virtual void visit(const ConsignedVehicle &vehicle) = 0;

protected:
    VehicleVisitor() = default;
    VehicleVisitor(const VehicleVisitor &) = default;
    VehicleVisitor &operator=(const VehicleVisitor &) = default;
    VehicleVisitor(VehicleVisitor &&) = default;
    VehicleVisitor &operator=(VehicleVisitor &&) = default;
};

} // namespace domain

#endif // DOMAIN_VEHICLEVISITOR_H
