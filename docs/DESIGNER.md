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
