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
├── include/ + src/                  CÓDIGO, por capas (hexagonal). Dentro de cada capa, una carpeta por
│   │                                módulo (inventory, auth…) y common/ para lo que usan varios
│   │
│   ├── domain/                      NÚCLEO: reglas del negocio. Solo QtCore; no conoce a nadie
│   │   ├── common/                  Lo que usan varios módulos
│   │   │   ├── value_objects/       enums, CatalogRef, ValidationResult, FileFacts
│   │   │   └── rules/
│   │   │       └── cashpaymentlimit  Tope LFPIORPI: 3,210 UMA en pagos en efectivo
│   │   └── inventory/               Módulo Inventario
│   │       ├── model/               Entidades con sus invariantes
│   │       │   ├── vehicle           Vehicle (abstracta): datos comunes, validate() como Template Method, contrato
│   │       │   ├── counterparty      Vendedor o propietario (contraparte)
│   │       │   ├── vehicleconditions Especificaciones del Paso 2 (combustible, transmisión, A.C.…)
│   │       │   ├── inspection(item)  Checklist de condición
│   │       │   ├── vehiclebuilder    Único que decide qué subclase crear; valida por paso
│   │       │   ├── vehiclevisitor    Visitor para tratar cada rama sin preguntar su tipo
│   │       │   ├── acquisition/
│   │       │   │   └── acquiredvehicle   Rama compra: precios, tipo y método de pago, tope de efectivo
│   │       │   └── consignment/
│   │       │       └── consignedvehicle  Rama consignación: precio base y comisión
│   │       ├── value_objects/       VehicleImage, VehicleDocument, ContractData
│   │       └── rules/
│   │           └── uploadformatpolicy  Formatos admitidos de fotos y documentos
│   │
│   ├── application/                 NÚCLEO: casos de uso. Solo QtCore; no conoce adaptadores ni widgets.
│   │   │                            En cada módulo: dto/ (datos que cruzan la frontera, uno por caso de uso),
│   │   │                            ports/ (interfaces que el núcleo necesita de afuera: los "repositorios")
│   │   │                            y services/ (los casos de uso)
│   │   ├── common/
│   │   │   ├── dto/                 catalogoptiondto (opción de un catálogo), uploaddtos (archivos)
│   │   │   └── ports/               filestorage: todo el acceso a disco (almacén, miniaturas, temporales)
│   │   ├── auth/                    Inicio de sesión
│   │   │   ├── dto/                 authdtos
│   │   │   ├── ports/               userdirectory, passwordhasher
│   │   │   └── services/            authenticationservice
│   │   └── inventory/               Módulo Inventario
│   │       ├── dto/                 inventorydtos: filtro y tarjeta de la rejilla
│   │       ├── ports/               inventoryreader
│   │       ├── services/            inventoryservice: consultar el inventario con sus miniaturas
│   │       └── registration/        Registrar un vehículo (las dos ramas)
│   │           ├── dto/             registrationdtos (lo que captura cada paso y el resultado),
│   │           │                    catalogdtos (catálogos, checklist y UMA del asistente)
│   │           ├── ports/           vehiclerepository, referencedatareader, contractgenerator
│   │           └── services/        vehicleregistrationservice (valida, revisa VIN, copia archivos, guarda,
│   │                                deshace lo copiado si algo falla) y registrationmapping (privado)
│   │
│   ├── adapters/                    Implementan los puertos. Primero la tecnología (cada una es su
│   │   │                            biblioteca, con solo su módulo de Qt), luego el módulo
│   │   ├── persistence/             PostgreSQL (Qt Sql)
│   │   │   ├── connectionpool        Una conexión por hilo; en Debug prohíbe SQL en el hilo de la interfaz
│   │   │   ├── auth/                sqluserdirectory (usuarios), devseeder (usuarios de prueba, solo desarrollo)
│   │   │   └── inventory/           sqlvehiclerepository (guardado transaccional), sqlcounterpartyrepository,
│   │   │                            sqlinventoryreader (rejilla), sqlreferencedatareader (catálogos y UMA)
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

**Módulos:** cada menú de la app (Inventario, Comercial, Finanzas, Reportes) es un módulo con su carpeta dentro de cada capa. Comercial, Finanzas y Reportes nacen con su primera pantalla. Lo de `common/` no depende de ningún módulo; un módulo puede usar `common/` y el dominio de otro módulo (las ventas, por ejemplo, van a usar `Vehicle`).
