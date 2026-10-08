# desktop-win-car-dealership

Aplicación de escritorio para Windows de **DE LA HOZ Autos Seminuevos**. Hecha en Qt 6 Widgets y C++17, sobre PostgreSQL. Hoy cubre el inicio de sesión, el inventario y el registro de vehículos por compra o consignación, con contrato en PDF.

## Requisitos

- Qt 6.11 con el kit **MinGW 64-bit** (Qt Creator o CLion).
- Docker Desktop, para PostgreSQL.

## Cómo correrla

1. Copia `.env.example` a `.env`. Para desarrollo sirven los valores tal cual.
2. Levanta la base:

   ```
   docker compose up -d
   ```

   - El esquema, los catálogos y la UMA vigente se crean con los scripts de `init-db/` la primera vez que el volumen está vacío.
   - Para recrearla desde cero: `docker compose down -v && docker compose up -d`.
3. Abre `CMakeLists.txt` en Qt Creator con el kit MinGW y ejecuta. Con `SEED_TEST_USERS=true` existen los usuarios `admin` / `Admin123!` y `vendedor` / `Vendedor123!`.

## Pruebas

```
cmake --build <carpeta de build>
ctest --test-dir <carpeta de build> --output-on-failure
```

- **Unitarias:**
  - el dominio;
  - los casos de uso con puertos falsos;
  - los presenters con vistas falsas;
  - humo de las pantallas con widgets reales.
- **Prueba `architecture`:** revisa que ninguna capa dependa de otra que no le toca.
- **Integración con PostgreSQL (opcional):** se configura con `-DSEMINUEVOS_DB_TESTS=ON` y se corre con `ctest -L db`.

## Documentación

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): la arquitectura hexagonal, qué va en cada carpeta, las reglas de dependencia, los hilos y las recetas para agregar campos y pantallas.
- [`docs/DESIGNER.md`](docs/DESIGNER.md): cómo editar las pantallas en Qt Designer y promover los componentes propios.
- [`resources/styles/README.md`](resources/styles/README.md): el sistema de diseño (colores, tipografía, componentes).
