# Estructura del proyecto

```
seminuevos/
│
├── main.cpp                         Punto de entrada: crea la QApplication y arranca la raíz de composición
├── CMakeLists.txt                   Build: una biblioteca estática por capa + el ejecutable + las pruebas
├── docker-compose.yml               PostgreSQL (y Adminer) para desarrollo
├── .env.example                     Plantilla del .env: conexión, usuarios de prueba, almacén, navegación libre
├── desktop-win-car-dealership_es_MX.ts   Traducciones (Qt Linguist)
├── README.md                        Cómo correr la app y las pruebas
│
├── cmake/                           Reglas del build y de la arquitectura
│   ├── SeminuevosLayers.cmake       Helper: una capa = una carpeta = una biblioteca con solo sus módulos de Qt
│   ├── CheckArchitecture.cmake      Prueba `architecture`: quién puede incluir a quién, disco en vistas, .ui limpios
│   └── architecture-allowlist.txt   Excepciones conocidas a la prueba (hoy vacío; solo puede encogerse)
│
├── docs/                            Guías del equipo
│   └── ESTRUCTURA.md                Este archivo
│
├── init-db/                         Scripts que Docker corre al crear la base (en orden por su número)
│   ├── 00_schema.sql                Tablas, llaves y CHECKs
│   ├── 10…14_*_cat.sql              Catálogos: marcas, combustibles, tipos/subtipos, checklist, mantenimientos
│   └── 90_global_configurations.sql Parámetros globales (UMA vigente)
│
├── resources/                       Todo lo que se compila dentro del ejecutable (Resources.qrc)
│   ├── Resources.qrc                Índice de recursos (:/icons, :/styles, :/templates, :/resources/images)
│   ├── icons/                       Íconos PNG de botones y menú
│   ├── images/                      Logo y fondo del login
│   ├── styles/                      Hojas de estilo
│   │   ├── global-style-clean.qss   Hoja global de la app (se aplica al entrar)
│   │   ├── login.qss                Hoja propia del login
│   │   ├── vehicle-card.qss         Hoja propia de la tarjeta del inventario
│   │   └── README.md                Sistema de diseño: colores, tipografía, componentes
│   ├── templates/                   Plantilla HTML del contrato (se imprime a PDF)
│   └── request_issuance_cfdi.html   Formulario de solicitud de CFDI (autofactura)
│
├── include/ + src/                  CÓDIGO, por capas (hexagonal)
│   │
│   ├── domain/                      NÚCLEO: reglas del negocio. Solo QtCore; no conoce a nadie
│   │   ├── model/                   Entidades con sus invariantes
│   │   │   ├── vehicle               Vehicle (abstracta): datos comunes, validate() como Template Method, contrato
│   │   │   ├── acquiredvehicle       Rama compra: precios, tipo y método de pago, tope de efectivo
│   │   │   ├── consignedvehicle      Rama consignación: precio base y comisión
│   │   │   ├── counterparty          Vendedor o propietario (contraparte)
│   │   │   ├── vehicleconditions     Especificaciones del Paso 2 (combustible, transmisión, A.C.…)
│   │   │   ├── inspection(item)      Checklist de condición
│   │   │   ├── vehiclebuilder        Único que decide qué subclase crear; valida por paso
│   │   │   └── vehiclevisitor        Visitor para tratar cada rama sin preguntar su tipo
│   │   ├── value_objects/           Valores sin identidad: enums, CatalogRef, ValidationResult, ContractData,
│   │   │                            FileFacts, VehicleImage, VehicleDocument
│   │   └── rules/                   Reglas que no son de una sola entidad
│   │       ├── cashpaymentlimit      Tope LFPIORPI: 3,210 UMA en pagos en efectivo
│   │       └── uploadformatpolicy    Formatos admitidos de fotos y documentos
│   │
│   ├── application/                 NÚCLEO: casos de uso. Solo QtCore; no conoce adaptadores ni widgets
│   │   ├── dto/                     Datos que cruzan la frontera, uno por caso de uso
│   │   │   ├── authdtos              Sesión e inicio de sesión
│   │   │   ├── registrationdtos      Lo que captura cada paso del asistente y el resultado del registro
│   │   │   ├── catalogdtos           Opciones de catálogo, checklist y UMA
│   │   │   ├── inventorydtos         Filtro y tarjeta del inventario
│   │   │   └── uploaddtos            Formatos, revisión de archivos y archivos temporales
│   │   ├── ports/                   Interfaces que el núcleo necesita de afuera (los "repositorios")
│   │   │   ├── vehiclerepository     Guardar una unidad y revisar VIN duplicado
│   │   │   ├── inventoryreader       Leer el inventario
│   │   │   ├── referencedatareader   Catálogos y UMA
│   │   │   ├── userdirectory         Buscar usuarios para el login
│   │   │   ├── filestorage           Todo el acceso a disco (almacén, miniaturas, temporales)
│   │   │   ├── contractgenerator     Generar el PDF del contrato
│   │   │   └── passwordhasher        Hash de contraseñas
│   │   └── services/                Casos de uso
│   │       ├── authenticationservice     Iniciar sesión
│   │       ├── vehicleregistrationservice Registrar un vehículo (valida, revisa VIN, copia archivos, guarda,
│   │       │                             deshace lo copiado si algo falla) y apoyos del asistente
│   │       ├── registrationmapping   (privado) DTOs del asistente → VehicleBuilder
│   │       └── inventoryservice      Consultar el inventario con sus miniaturas
│   │
│   ├── adapters/                    Implementan los puertos con tecnología concreta (cada uno es su biblioteca)
│   │   ├── persistence/             PostgreSQL (Qt Sql)
│   │   │   ├── connectionpool        Una conexión por hilo; en Debug prohíbe SQL en el hilo de la interfaz
│   │   │   ├── sqlvehiclerepository  Guardado transaccional de la unidad
│   │   │   ├── sqlcounterpartyrepository  Contrapartes (lo usa el repositorio de vehículos)
│   │   │   ├── sqlinventoryreader    Proyección de lectura para la rejilla
│   │   │   ├── sqlreferencedatareader Catálogos y UMA
│   │   │   ├── sqluserdirectory      Usuarios
│   │   │   └── devseeder             Usuarios de prueba (solo desarrollo)
│   │   ├── storage/                 Disco local (localfilestorage): almacén por VIN, miniaturas, temporales
│   │   ├── contract/                PDF del contrato: pdfcontractgenerator, datos de la agencia, número a letras
│   │   └── security/                Hash PBKDF2 de contraseñas
│   │
│   ├── presentation/                Lógica de pantalla SIN widgets (solo QtCore, se prueba sin ventanas)
│   │   ├── presenters/              Presenters (MVP) y las interfaces I*View que cada uno necesita
│   │   │   ├── loginpresenter + iloginview
│   │   │   ├── inventorypresenter + iinventoryview
│   │   │   ├── vehiclewizardpresenter + ivehiclewizardview     Navegación, validación en vivo y registro
│   │   │   ├── wizardsteppresenter + istepview                 Base abstracta de un paso
│   │   │   ├── vehicle{details,conditions,files}presenter      Un presenter por paso
│   │   │   ├── ivehicle{details,conditions,files}view          Lo que cada paso necesita de su vista
│   │   │   └── invoiceattachment                               Regla de la autofactura (factura después del CFDI)
│   │   ├── navigation/              wizardnavigator: qué paso se ve, cuáles son válidos, palomitas y bloqueos
│   │   ├── tasks/                   taskrunner (abstracto) y pooledtaskrunner: trabajo fuera del hilo de la interfaz
│   │   └── views/                   WIDGETS (Qt Widgets): implementan las I*View; no tocan base ni disco
│   │       ├── shell/               mainwindow: la cáscara (menú lateral, barra superior, pila de páginas)
│   │       ├── login/               loginwindow
│   │       ├── inventory/           inventoryview (página del inventario) y vehicleitemlist (tarjeta)
│   │       ├── wizard/              vehiclewizardview (marco) y los tres pasos: vehicledetailsview,
│   │       │                        vehicleconditionsview, vehiclefilesview; conditionchecklistrow (fila dinámica)
│   │       ├── components/          Componentes promovibles en Designer: sidebarmenuitem, navtabitem,
│   │       │                        outlinebutton, aspectratioimagelabel, wizardstepper (+ statefultextbutton, base)
│   │       └── support/             formsupport: marcar errores, foco, vigilar ediciones, llenar combos
│   │
│   └── app/                         Raíz de composición: lo único que conoce todas las capas
│       ├── appsettings              Lee el .env (conexión, almacén, banderas de desarrollo)
│       └── compositionroot          Crea adaptadores, servicios y presenters, y decide qué ventana se ve
│
├── src/ui/                          Formularios de Qt Designer (uno por vista)
│   ├── loginwindow.ui               Inicio de sesión
│   ├── mainwindow.ui                Cáscara (la página del inventario va promovida)
│   ├── inventoryview.ui             Pestañas, filtros y lista del inventario
│   ├── vehicleitemlist.ui           Tarjeta del inventario
│   ├── vehiclewizardview.ui         Marco del asistente (stepper, pasos promovidos, botones)
│   ├── vehicledetailsview.ui        Paso 1: vehículo, contraparte y operación
│   ├── vehicleconditionsview.ui     Paso 2: especificaciones y checklist
│   └── vehiclefilesview.ui          Paso 3: galería y documentos
│
├── src/README.md                    Guía anterior de backend (3-Tier); ya no refleja la arquitectura actual
│
└── tests/                           Pruebas QtTest; cada una enlaza solo la capa que prueba
    ├── CMakeLists.txt               Registro de pruebas en ctest
    ├── domain/                      Reglas del negocio: vehículos, polimorfismo, tope de efectivo, formatos
    ├── application/                 Casos de uso con puertos falsos (login, registro)
    ├── presentation/                Presenters con vistas falsas + humo de las pantallas con widgets reales
    ├── adapters/                    Disco real en carpeta temporal; integración con PostgreSQL (opcional)
    └── support/                     Dobles de prueba compartidos
        ├── fakes.h                  Puertos falsos, TaskRunners de prueba, vistas falsas de login e inventario
        ├── wizardfakes.h            Vistas falsas del asistente
        └── registrationfixtures.h   Una captura válida del asistente
```

**Fuera del repositorio** (las ignora git): `build/` (compilación), `storage/` (fotos y documentos guardados, por VIN) y `.env` (configuración local).

**Regla de dependencias, en una línea:** `presentation → application → domain ← adapters`, y `app/` arma todo. Las vistas nunca tocan `adapters/`, SQL ni el disco.
