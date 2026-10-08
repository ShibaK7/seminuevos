# Arquitectura de seminuevos (v1)

> **Versión 1, de la mudanza a carpetas hexagonales.**
>
> - La versión completa llega al terminar el refactor e incluye recetas, hilos y la tabla "De 3-Tier a hexagonal".
> - Mientras tanto, lo que aquí se describe como destino puede seguir parcialmente en el código viejo.
> - `cmake/architecture-allowlist.txt` lista exactamente qué falta y qué commit lo resuelve.

La app sigue una **arquitectura hexagonal**, también llamada de puertos y adaptadores:
- **El núcleo** es el dominio más la aplicación. No sabe nada de Qt Widgets, de PostgreSQL ni del disco.
- **Los adaptadores** conectan ese núcleo con el mundo:
  - de salida: la base de datos, el almacenamiento de archivos, el PDF del contrato y el hash de contraseñas;
  - de entrada: la interfaz gráfica.

## Carpetas

`include/` (headers) y `src/` (fuentes) tienen el mismo árbol. Los `.ui` de Qt Designer viven en `src/ui/`.

| Carpeta | Qué contiene | Qt permitido |
|---|---|---|
| `domain/model/` | Entidades con sus invariantes: `Vehicle` (abstracta) y sus ramas, `Counterparty`, `VehicleConditions`, `Inspection`, `VehicleBuilder` | QtCore |
| `domain/value_objects/` | Valores sin identidad: enums, `CatalogRef`, `ValidationResult`, imágenes y documentos | QtCore |
| `domain/rules/` | Reglas que no pertenecen a una sola entidad (p. ej. el tope LFPIORPI de pago en efectivo) | QtCore |
| `application/dto/` | Datos que cruzan la frontera, **uno por caso de uso** | QtCore |
| `application/ports/` | Interfaces que el núcleo necesita de afuera (repositorios, almacenamiento, PDF…) | QtCore |
| `application/services/` | Casos de uso | QtCore |
| `adapters/persistence/` | PostgreSQL: `ConnectionPool` y los `Sql*` | Sql |
| `adapters/storage/` | Archivos en disco (`LocalFileStorage`) | Gui |
| `adapters/contract/` | PDF del contrato | Gui, PrintSupport |
| `adapters/security/` | Hash de contraseñas (PBKDF2) | Network |
| `presentation/presenters/` | Presenters y sus interfaces de vista `I*View` | QtCore |
| `presentation/navigation/` | Estado de navegación (`WizardNavigator`) | QtCore |
| `presentation/tasks/` | Trabajo fuera del hilo de la interfaz | QtCore |
| `presentation/views/` | Widgets: implementan `I*View`; componentes promovibles en Designer | Widgets |
| `app/` | Raíz de composición: crea y conecta todo | todo |

**Equivalencias con los nombres de siempre:**

| Término | Dónde vive |
|---|---|
| model | `domain/model/` |
| dto | `application/dto/` |
| repository | interfaz en `application/ports/` + implementación `Sql*` en `adapters/persistence/` |
| service | `application/services/` |
| view | `presentation/views/` + `src/ui/` |
| presenter | `presentation/presenters/` |

## Regla de dependencias

```
presentation ─────► application ─────► domain
                          ▲
                          │
                       adapters   (implementan las interfaces de application/ports)
```

- **domain** no conoce a nadie.
- **application** no conoce `presentation` ni `adapters`.
- **adapters** solo ven `application/ports` (con sus DTO) y `domain`, y ninguno ve a otro adaptador.
- **presentation** usa `application/services` y `application/dto`. Del dominio solo usa `domain/value_objects`.
- **Las vistas** no tocan repositorios, SQL, adaptadores ni el disco:
  - piden rutas al usuario;
  - pintan los bytes que les da el presenter;
  - abren con `QDesktopServices` lo que el presenter les indica.

## Cómo se hace cumplir

1. **Compilador.** Cada capa es una biblioteca estática (`cmake/SeminuevosLayers.cmake`) que solo enlaza sus módulos de Qt. Por ejemplo, `#include <QSqlQuery>` en el dominio no compila.
2. **Enlazador.** Cada prueba enlaza solo la biblioteca de su capa.
3. **Prueba `architecture`** (`cmake/CheckArchitecture.cmake`, corre con `ctest`). Revisa:
   - los includes entre capas;
   - los headers de Qt de módulos prohibidos;
   - `QObject` en el dominio;
   - el acceso a disco desde las vistas;
   - los slots `on_x_y`;
   - las conexiones o estilos escritos dentro de los `.ui`.

## Convenciones

- **Includes:** siempre con la ruta de la capa (`"domain/model/vehicle.h"`). Los formularios van como `"ui_x.h"`.
- **Guardas:** la ruta del header en mayúsculas (`DOMAIN_MODEL_VEHICLE_H`).
- **Comentarios:** en español y explicando el porqué. Los identificadores van en inglés.
- **Commits:** en inglés imperativo.

## Pruebas

```
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```
