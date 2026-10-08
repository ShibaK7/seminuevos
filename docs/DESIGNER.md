# Qt Designer en seminuevos (v1)

> **Versión 1, de los componentes de navegación promovibles.** La guía completa (formularios, buddies, checklist de PR) llega al final del refactor.

Los componentes propios se ponen en Designer con **"Promover a…"**: no hay plugin de Designer, porque Qt Creator está compilado con MSVC y el kit del proyecto es MinGW.

## Cómo promover un widget

1. Arrastra el widget base de la tabla (por ejemplo, un `QPushButton`).
2. Clic derecho > **Promover a…**
3. **Nombre de la clase promovida:** el de la tabla (`SidebarMenuItem`).
4. **Archivo de encabezado:** `presentation/views/components/sidebarmenuitem.h`, con esa ruta exacta.
5. **No** marques "Global include".
6. Agregar > Promover.
7. Borra el texto `PushButton` que Designer precarga. En estos botones el texto visible es la propiedad `title`, no `text`.

## Componentes promovibles

| Componente | Widget base | Propiedades dinámicas | Lo fija el constructor (no lo pongas en el .ui) |
|---|---|---|---|
| `SidebarMenuItem` | QPushButton | `title`, `leadingIcon` | `class`, `checkable`, `autoExclusive` |
| `NavTabItem` | QPushButton | `title` | `class`, `checkable`, `autoExclusive` |
| `OutlineButton` | QPushButton | `title`, `leadingIcon` (vacío = sin icono) | `class`, alto fijo |
| `AspectRatioImageLabel` | QLabel | `sourcePath` (solo recursos `:/`) | |
| `WizardStepper` | QWidget | `steps` (StringList) | estados de los pasos |

`StatefulTextButton` es la base de los tres botones y **no** se promueve directamente.

## Propiedades dinámicas

Se agregan con el **+** del editor de propiedades, de tipo **String** (o **StringList** para `steps`).

- **Con "Translatable" desmarcado** (`notr="true"` en el XML): `class`, `field`, `branch`, `leadingIcon`, `sourcePath`. Son claves y rutas, no texto para el usuario. `class` además debe llegar antes de que se aplique la hoja de estilos, y las traducibles se asignan al final de `setupUi`.
- **Traducibles:** `title` y `steps`, que son texto visible.
- **Solo en ejecución, nunca en el .ui:** `hasError`, `stepState`, `itemState`, `itemChecked`, `status`. Las pone el código.
- **`checked` en los botones promovidos:** no lo marques. Designer los ve como `QPushButton` no checkable y al guardar lo reescribe como `false`. La selección inicial la pone el `.cpp`.

## Reglas

- **Conexiones:** siempre con `connect` explícito en el `.cpp`, con punteros a miembro. Nada de "Ir al slot…" ni slots `on_x_y`, y ninguna conexión en el editor de señales y slots. La prueba `architecture` lo revisa.
- **Estilos:** nada de `styleSheet` en el `.ui`; todo va en `resources/styles/global-style-clean.qss`. Para una tarjeta, usa la propiedad `class` = `card`. La prueba `architecture` lo revisa.
- **Un dueño por .ui a la vez:** Designer reescribe el XML entero al guardar, así que dos personas editando el mismo `.ui` siempre chocan.
- **Lo que ves no es el aspecto real:** Designer muestra la clase base (un `QPushButton` vacío), no el componente. Cómo se ve de verdad se revisa ejecutando la app.

## Vista previa con la hoja de estilos

1. Ve a Opciones > Designer > Forms > Preview Configuration.
2. Elige `resources/styles/global-style-clean.qss` como Style sheet.
3. Abre la vista previa con Ctrl+R.

Ni así se ve el aspecto final de los promovidos, porque la vista previa también usa la clase base.

## Asistente de registro

Ya están en `.ui`:

- `vehiclewizardview.ui`: el marco (título, pila con los tres pasos como
  widgets promovidos, aviso de error y botones Cancelar / Imprimir Contrato /
  primario).
- `vehicledetailsview.ui` (Paso 1): las dos tarjetas con todos los campos.
- `vehicleconditionsview.ui` (Paso 2): la tarjeta de especificaciones y el
  contenedor del checklist.
- `vehiclefilesview.ui` (Paso 3): las tarjetas de galería y documentos, sus
  avisos, el área de scroll y el botón "+ Agregar fotografía".

Siguen en código, y por qué:

- **Filas del checklist** (Paso 2): salen del catálogo de la base, así que no
  se conocen al diseñar. El `.ui` solo trae `checklistContent` vacío.
- **Filas de fotos y de documentos** (Paso 3): se rehacen cada vez que se
  agrega o quita un archivo, dentro de `galleryLayout` y `documentsLayout`.
- **La sombra de las tarjetas** (`formsupport::applyFloatingShadow`):
  Designer no puede declarar un `QGraphicsEffect`.
- **El nombre `cardPanel` de las tarjetas**: el QSS las pinta por ese nombre,
  pero un formulario no admite dos widgets con el mismo; en el `.ui` cada
  tarjeta tiene nombre propio y el constructor la renombra.
- **Los combos de valores cerrados** (Tipo Operación, Tipo Pago, Método Pago,
  Tipo Factura, Transmisión, Cristales, A.C.): se llenan desde el dominio,
  que es quien decide qué texto acepta la base. En el `.ui` van vacíos.
  Cilindros e Interiores sí traen sus opciones en el `.ui`.
- **El stepper**: recibe los títulos de los pasos por constructor; entra por
  el hueco `stepperSlot` del marco.
- **Fechas y topes que dependen de hoy** (fecha de la operación, año modelo).

### La propiedad `field`

Cada widget de captura lleva una propiedad dinámica `field` con la clave con
la que el dominio reporta sus errores (`serialNumber`,
`counterparty.fullName`, `conditions.transmission`...). Con ella el asistente
encuentra el widget de cada error.

Si a un widget se le quita `field`, o se le cambia la clave, el campo **deja
de marcarse en rojo y de recibir el foco cuando falla**, y no truena nada que
lo avise. Al copiar o crear un campo en Designer, revisa que la lleve con la
clave correcta. La prueba `tst_vehiclewizardview` comprueba las claves del
Paso 1.

Los campos propios de cada rama (Adquisición / Consignación) ocupan filas
completas de la rejilla: al ocultar una rama, sus filas colapsan enteras. Si
se mueven, conserva eso; las listas de qué se oculta en cada rama están en el
constructor de `VehicleDetailsView`.
