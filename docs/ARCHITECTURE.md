# Arquitectura de seminuevos

La app sigue una **arquitectura hexagonal**, también llamada de puertos y adaptadores:
- **El núcleo** es el dominio más la aplicación. No sabe nada de Qt Widgets, de PostgreSQL ni del disco.
- **Los adaptadores** conectan ese núcleo con el mundo:
  - de salida: la base de datos, el almacenamiento de archivos, el PDF del contrato y el hash de contraseñas;
  - de entrada: la interfaz gráfica, que sigue el patrón **MVP con vista pasiva** (Model-View-Presenter).

## Carpetas

`include/` (headers) y `src/` (fuentes) tienen el mismo árbol. Los `.ui` de Qt Designer viven en `src/ui/`.

| Carpeta | Qué contiene | Qt permitido |
|---|---|---|
| `domain/model/` | Entidades con sus invariantes: `Vehicle` (abstracta) y sus ramas, `Counterparty`, `VehicleConditions`, `Inspection`, `VehicleBuilder` | QtCore |
| `domain/value_objects/` | Valores sin identidad: enums, `CatalogRef`, `ValidationResult`, `ContractData`, `FileFacts`, imágenes y documentos | QtCore |
| `domain/rules/` | Reglas que no pertenecen a una sola entidad: el tope LFPIORPI de pago en efectivo (`CashPaymentLimit`) y los formatos de archivo admitidos (`UploadFormatPolicy`) | QtCore |
| `application/dto/` | Datos que cruzan la frontera, **uno por caso de uso** | QtCore |
| `application/ports/` | Interfaces que el núcleo necesita de afuera | QtCore |
| `application/services/` | Casos de uso | QtCore |
| `adapters/persistence/` | PostgreSQL: `ConnectionPool` y los `Sql*` | Sql |
| `adapters/storage/` | Todo el acceso a disco (`LocalFileStorage`) | Gui |
| `adapters/contract/` | PDF del contrato | Gui, PrintSupport |
| `adapters/security/` | Hash de contraseñas (PBKDF2) | Network |
| `presentation/presenters/` | Presenters y las interfaces de vista `I*View` que cada uno necesita | QtCore |
| `presentation/navigation/` | Estado de navegación del asistente (`WizardNavigator`) | QtCore |
| `presentation/tasks/` | Trabajo fuera del hilo de la interfaz (`TaskRunner`) | QtCore |
| `presentation/views/` | Widgets que implementan `I*View`; `components/` (promovibles en Designer) y `support/` (`formsupport`) | Widgets |
| `src/ui/` | Formularios de Qt Designer (ver `docs/DESIGNER.md`) | — |
| `app/` | Raíz de composición: crea y conecta todo | todo |

Cada capa es una biblioteca estática: `seminuevos_domain`, `seminuevos_application`, `seminuevos_presentation` (presenters, navigation y tasks), `seminuevos_views` (vistas y formularios), y una por adaptador (`seminuevos_persistence`, `_storage`, `_contract`, `_security`). El ejecutable solo compila `main.cpp`, `app/` y los recursos.

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

| Capa | Puede usar | No puede incluir |
|---|---|---|
| `domain` | solo `domain/` y QtCore | QtWidgets, QtGui, QtSql, QtNetwork, `application`, `presentation`, `adapters`, `app` |
| `application` | `domain` | QtWidgets, QtGui, QtSql, `adapters`, `presentation`, `app` |
| `adapters` | `application/ports` (y los DTO que esos puertos usan), `domain` | `application/services`, `presentation`, `app`, otro adaptador |
| `presentation` (presenters, navigation, tasks) | `application/services`, `application/dto`, `domain/value_objects` | `application/ports`, `adapters`, `domain/model`, `domain/rules`, `presentation/views`, QtWidgets, QtGui, QtSql |
| `presentation/views` | lo anterior + `presentation/*` + `ui_*.h` | `application/services`, repositorios, `adapters`, QtSql, **acceso a disco** |
| `app` | todo | — |

**"Las vistas no tocan disco"** quiere decir que no usan `QFile`, `QDir`, `QFileInfo`, `QTemporaryFile`, `QSaveFile`, `QMimeDatabase`, `QImageReader` ni `QStandardPaths`, ni cargan un `QPixmap` desde una ruta. Lo que sí pueden hacer:
- pedir una ruta con `QFileDialog` o por arrastrar y soltar;
- abrir con `QDesktopServices` una ruta que les da el presenter;
- pintar con `QPixmap::loadFromData` los bytes que les da el presenter;
- usar recursos compilados (`:/...`).

## Cómo se hace cumplir

1. **Compilador.** Cada capa es una biblioteca estática (`cmake/SeminuevosLayers.cmake`) que solo enlaza sus módulos de Qt. Un `#include <QSqlQuery>` en el dominio, o un `QWidget` en un presenter, no compila.
2. **Enlazador.** Cada prueba enlaza solo la biblioteca de su capa.
3. **Prueba `architecture`** (`cmake/CheckArchitecture.cmake`, corre con `ctest`). Revisa:
   - los includes entre capas (la tabla de arriba);
   - los headers de Qt de módulos prohibidos;
   - `QObject` en el dominio y en la aplicación;
   - el acceso a disco desde las vistas;
   - los slots `on_x_y` (conexión automática de uic);
   - las conexiones (`<connection>`) y los `styleSheet` escritos dentro de los `.ui`.

   Las excepciones conocidas vivían en `cmake/architecture-allowlist.txt`, cada una con el commit que la quitaba. Hoy está **vacío**, y así debe quedarse: una violación nueva se corrige, no se agrega.

## Reglas para DTOs

- **Un DTO por caso de uso.** Nada de un `VehicleDto` genérico que copie los ~30 campos de `Vehicle`.
- **Registro:** `VehicleDetailsDto`, `VehicleConditionsDto` y `VehicleFilesDto` (lo que captura cada paso del asistente) y `VehicleRegistrationDto`, que **compone** esos tres en vez de copiar campos. La respuesta es `RegistrationResult` (folio, errores por campo y `ContractData`).
- **Lectura:** `RegistrationLookupsDto` (catálogos y UMA), `CatalogOptionDto`, `ChecklistItemDto`, `InventoryFilterDto`, `InventoryItemDto` (solo lo que pinta la tarjeta).
- **Sesión:** `SessionDto` y `LoginResult`.
- **Archivos:** `UploadFormatsDto`, `UploadCheckDto`, `TemporaryFileDto`.
- **Sin lógica ni validación:** son structs planos. Validar es trabajo del dominio. "Sin elegir" se expresa con `std::optional`.
- **No se reutilizan** en otro caso de uso solo porque "tienen los mismos campos".

## ¿Dónde va mi código?

| Quiero… | Va en… |
|---|---|
| una regla de negocio de una entidad ("el precio de venta debe ser mayor a 0") | el `validate()` de la entidad en `domain/model/` |
| una regla que cruza entidades o depende de un parámetro (tope de efectivo, formatos admitidos) | `domain/rules/` |
| un caso de uso nuevo ("vender un vehículo") | un servicio en `application/services/` con sus DTO en `application/dto/` |
| leer o escribir en PostgreSQL | un puerto en `application/ports/` y su `Sql*` en `adapters/persistence/` |
| leer o escribir archivos | el puerto `FileStorage` (`adapters/storage/LocalFileStorage`) |
| qué hace la pantalla cuando el usuario hace algo | el presenter de esa pantalla |
| cómo se ve la pantalla | el `.ui` en `src/ui/` (Designer) y `resources/styles/global-style-clean.qss`; el login y la tarjeta del inventario tienen además su hoja propia (`login.qss`, `vehicle-card.qss`) |
| un widget reutilizable que el equipo arrastre en Designer | `presentation/views/components/` (ver `docs/DESIGNER.md`) |
| crear y conectar un objeto nuevo | `app/compositionroot.cpp` |

## Hilos

- **Nada de SQL ni de disco en el hilo de la interfaz.** Los presenters mandan el trabajo con `TaskRunner::run(contexto, trabajo, alTerminar)`:
  - `trabajo` corre en un hilo del pool (`PooledTaskRunner`, dos hilos) y solo recibe DTO por valor y servicios;
  - `alTerminar` corre de vuelta en el hilo de la interfaz, y **solo si el contexto sigue vivo**: si el usuario cerró la pantalla, el resultado se descarta sin tronar.
- **Una conexión por hilo.** `ConnectionPool` le da a cada hilo su propia conexión (Qt no permite compartir una `QSqlDatabase` entre hilos). Si una operación falla, el adaptador descarta la conexión del hilo para que la siguiente abra otra.
- **En las pruebas** se usa `InlineTaskRunner` (corre todo en el momento) o `DeferredTaskRunner` (deja elegir el orden de las respuestas).
- **Excepciones conocidas en el hilo de la interfaz:** el arranque (ping a la base y usuarios de prueba), la revisión de formato de un archivo elegido (lee solo su encabezado), las miniaturas del Paso 3 y el PDF del contrato.

## MVP: cómo se arma una pantalla

```
 Widget (QWidget + IXView)  ──señales──►  XPresenter (QObject, QtCore)  ──►  XService
        ▲                                          │
        └──────────── métodos de IXView ───────────┘
```

- **La vista** implementa una interfaz `I*View` que define el presenter. No decide nada: avisa lo que hizo el usuario (señales) y pinta lo que le dicen (métodos de la interfaz).
- **El presenter** no conoce widgets, solo la interfaz. Por eso se prueba con una vista falsa (`tests/support/wizardfakes.h`).
- **La raíz de composición** crea la vista, crea el presenter con sus servicios y llama a `bind()` para conectarlos.

Ejemplos en el código:

| Pantalla | Vista | Presenter |
|---|---|---|
| Inicio de sesión | `LoginWindow` | `LoginPresenter` |
| Inventario | `MainWindow` (rejilla y filtros) | `InventoryPresenter` |
| Asistente de registro | `VehicleWizardView` + `VehicleDetailsView`, `VehicleConditionsView`, `VehicleFilesView` | `VehicleWizardPresenter` + un `WizardStepPresenter` por paso |

## Recetas

**Agregar un campo al Paso 1:**
1. Agrega el atributo y su validación en la entidad (`domain/model/`).
2. Agrega el campo al DTO (`application/dto/registrationdtos.h`) y al mapeo (`src/application/services/registrationmapping.cpp`).
3. Si se guarda, agrégalo al `Sql*` correspondiente y a `init-db/`.
4. En Designer, arrastra el widget y ponle la propiedad dinámica `field` con la clave que usa el dominio en sus errores. Así se marca en rojo solo.
5. Léelo en `VehicleDetailsView::details()`.

**Agregar un catálogo:**
1. Agrega el método al puerto `ReferenceDataReader` y a su `SqlReferenceDataReader` (y al fake de las pruebas).
2. Agrégalo a `RegistrationLookupsDto` y a `VehicleRegistrationService::loadLookups()`.
3. Llénalo en la vista con `formsupport::fillCombo`.

**Agregar una pantalla:** define `IXView` y `XPresenter` en `presentation/presenters/`, el widget en `presentation/views/` con su `.ui`, la prueba del presenter con una vista falsa, y conéctalos en `app/compositionroot.cpp`.

## De 3-Tier a hexagonal

La guía anterior (`src/README.md`) proponía tres capas. Esto es lo que cambió y por qué:

| Antes | Ahora | Por qué |
|---|---|---|
| Repositorios con métodos `static` | Puertos (interfaces) + adaptadores `Sql*` que reciben el pool | Se pueden sustituir por fakes en las pruebas y no dependen de un singleton |
| SQL en la UI para llenar combos | `ReferenceDataReader` | La vista no conoce la base; el SQL vive en un solo lugar |
| La vista arma el `VehicleBuilder` y un hilo copia archivos | `VehicleRegistrationService` | Un caso de uso con un solo dueño: valida, revisa el VIN, copia y guarda, y deshace lo copiado si algo falla |
| Validación repartida entre la vista y el dominio | Solo el dominio valida | Una sola fuente de verdad; la vista solo marca lo que el dominio reporta |
| Servicios y DTOs | Se conservan | Era lo compatible con hexagonal |
| Repositorios sin lógica | Se conserva | Igual que antes: la lógica es del dominio |

## Convenciones

- **Includes:** siempre con la ruta de la capa (`"domain/model/vehicle.h"`). Los formularios van como `"ui_x.h"`.
- **Guardas:** la ruta del header en mayúsculas (`DOMAIN_MODEL_VEHICLE_H`).
- **Comentarios:** en español y explicando el porqué. Los identificadores van en inglés.
- **Commits:** en inglés imperativo, uno por cambio; no mezclar una mudanza con lógica ni estilo con lógica.
- **Navegación libre del asistente** (para trabajar estilos sin llenar el formulario): `WIZARD_FREE_NAVIGATION=true` en el `.env`. No se comenta la validación.

## Pruebas

```
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```

Las de integración con PostgreSQL son opcionales: `-DSEMINUEVOS_DB_TESTS=ON` con `docker compose up -d`, y se corren con `ctest -L db`.
