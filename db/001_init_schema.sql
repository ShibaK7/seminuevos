-- ===========================================================================
-- SCHEMA: seminuevos (concesionaria de autos)
-- ===========================================================================
-- Solo estructura -- los datos van en los seeders de C++, no aquí.
--
-- *** ESTE SCRIPT ES DESTRUCTIVO: empieza borrando todas las tablas. ***
--
-- No lo ejecuta la app en cada arranque. SchemaInitializer compara la tabla
-- schema_version contra la constante kSchemaVersion del código y solo corre
-- este archivo cuando no coinciden. Para forzar la regeneración: sube
-- kSchemaVersion en include/db/schemainitializer.h y vuelve a compilar.
--
-- REGLAS DE FORMATO (de las que depende el parser de SchemaInitializer):
--   1. Una sola sentencia por bloque, terminada con ';' al final de su
--      última línea. Nada de 'DROP TABLE a; DROP TABLE b;' en un renglón.
--   2. Los comentarios van en líneas propias, empezando con '--'.
--   3. Nada de bloques $$ ... $$ (funciones, DO): el parser cortaría en el
--      primer ';' interno y produciría fragmentos inválidos.
-- ===========================================================================

-- ===========================================================================
-- 0. LIMPIEZA (hijos -> padres)
-- ===========================================================================
-- El CASCADE bastaría por sí solo, pero el orden explícito deja el bloque
-- legible como el inverso exacto de los CREATE de abajo.
--
-- OJO: schema_version NO se dropea aquí a propósito. La crea y la mantiene
-- SchemaInitializer; si este script la borrara, la app perdería la cuenta de
-- qué versión tiene instalada y volvería a resetear en cada arranque.

DROP TABLE IF EXISTS sale_payments CASCADE;
DROP TABLE IF EXISTS sale_financing CASCADE;
DROP TABLE IF EXISTS sales CASCADE;

DROP TABLE IF EXISTS vehicle_maintenance CASCADE;
DROP TABLE IF EXISTS vehicle_documents CASCADE;
DROP TABLE IF EXISTS vehicle_images CASCADE;
DROP TABLE IF EXISTS vehicle_inspection CASCADE;

-- LEGACY: reemplazada por vehicle_conditions_cat + vehicle_inspection.
-- Ya no se recrea; sin este DROP se quedaría huérfana en las bases que la
-- tienen, con datos que parecen buenos y ningún código que los escriba.
DROP TABLE IF EXISTS vehicle_condition_items CASCADE;

DROP TABLE IF EXISTS vehicle_conditions CASCADE;
DROP TABLE IF EXISTS vehicle_consignments CASCADE;
DROP TABLE IF EXISTS vehicle_acquisitions CASCADE;
DROP TABLE IF EXISTS vehicles CASCADE;

DROP TABLE IF EXISTS counterparty_documents CASCADE;
DROP TABLE IF EXISTS counterparties CASCADE;
DROP TABLE IF EXISTS users CASCADE;

DROP TABLE IF EXISTS operational_expenses CASCADE;
DROP TABLE IF EXISTS operational_expense_cat CASCADE;

DROP TABLE IF EXISTS vehicle_conditions_cat CASCADE;
DROP TABLE IF EXISTS vehicle_maintenance_cat CASCADE;
DROP TABLE IF EXISTS fuel_type_cat CASCADE;
DROP TABLE IF EXISTS brands_cat CASCADE;
DROP TABLE IF EXISTS vehicle_categories_cat CASCADE;

DROP TABLE IF EXISTS global_configurations CASCADE;

-- ===========================================================================
-- 1. CATÁLOGOS (van primero: el resto los referencia)
-- ===========================================================================

CREATE TABLE vehicle_categories_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    parent_id INTEGER REFERENCES vehicle_categories_cat(id) ON DELETE CASCADE,
    CONSTRAINT unique_name_per_parent UNIQUE NULLS NOT DISTINCT (name, parent_id)
);

CREATE TABLE brands_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL
);

CREATE TABLE fuel_type_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL
);

-- Catálogo del checklist de condición del Paso 2 del wizard. Lo siembra
-- ConditionCatalogSeeder en cada arranque (upsert por item_key) y la UI
-- construye sus renglones leyendo esta tabla: si queda vacía, el panel de
-- condiciones sale en blanco.
--
-- item_key es la llave estable del upsert. Sin ella la única llave natural
-- sería (category, element) -- texto visible --, y corregir una etiqueta
-- crearía una fila nueva en vez de actualizar la existente, dejando el
-- catálogo duplicado y las inspecciones viejas apuntando a la fila muerta.
--
-- negative_label es la etiqueta del estado negativo, que varía por ítem
-- ("Con fallas", "Deteriorada", "Gastadas"). Es presentación pura: la BD
-- guarda un booleano, no este texto.
--
-- sort_order es global y creciente a lo largo de todo el catálogo (no se
-- reinicia por categoría), para que un simple ORDER BY entregue los ítems ya
-- agrupados y con los grupos en el orden correcto.
CREATE TABLE vehicle_conditions_cat (
    id SERIAL PRIMARY KEY,
    item_key VARCHAR(50) UNIQUE NOT NULL,
    category VARCHAR(50) NOT NULL,
    element VARCHAR(50) NOT NULL,
    negative_label VARCHAR(50) NOT NULL DEFAULT 'Con fallas',
    sort_order INTEGER NOT NULL DEFAULT 0,
    CONSTRAINT unique_category_element UNIQUE (category, element)
);

CREATE TABLE vehicle_maintenance_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL
);

CREATE TABLE operational_expense_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL,
    description TEXT
);

-- ===========================================================================
-- 2. USUARIOS Y CONTRAPARTES
-- ===========================================================================

CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(100) NOT NULL,
    role VARCHAR(30) NOT NULL CHECK (role IN ('Administrador', 'Vendedor')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Una sola tabla para todos los roles (vendedor, propietario, comprador,
-- aval): el rol es contextual, no un tipo. La misma persona puede vender un
-- auto hoy y comprar otro mañana.
CREATE TABLE counterparties (
    id SERIAL PRIMARY KEY,
    full_name VARCHAR(100) NOT NULL,
    national_id VARCHAR(50),
    street_address TEXT,
    suburb VARCHAR(100),
    locality VARCHAR(100),
    state VARCHAR(50),
    postal_code VARCHAR(10),
    phone VARCHAR(20),
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE counterparty_documents (
    id SERIAL PRIMARY KEY,
    counterparty_id INTEGER NOT NULL REFERENCES counterparties(id) ON DELETE CASCADE,
    document_type VARCHAR(50) NOT NULL,
    file_path TEXT NOT NULL,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_active_counterparty_doc UNIQUE (counterparty_id, document_type)
);

-- ===========================================================================
-- 3. INVENTARIO DE VEHÍCULOS
-- ===========================================================================
-- Class Table Inheritance: vehicles es la tabla base y acquisition_type el
-- discriminador; vehicle_acquisitions y vehicle_consignments son las
-- subtablas. En C++ esto se refleja como Vehicle (abstracta) ->
-- AcquiredVehicle / ConsignedVehicle.

CREATE TABLE vehicles (
    folio SERIAL PRIMARY KEY,
    acquisition_type VARCHAR(20) NOT NULL CHECK (acquisition_type IN ('Adquisición', 'Consignación')),
    status VARCHAR(20) DEFAULT 'Disponible' CHECK (status IN ('Disponible', 'Apartado', 'Vendido')),
    vehicle_type_id INTEGER REFERENCES vehicle_categories_cat(id),
    subtype_id INTEGER REFERENCES vehicle_categories_cat(id),
    brand_id INTEGER REFERENCES brands_cat(id),
    model VARCHAR(50) NOT NULL,
    year_model INTEGER NOT NULL,
    color VARCHAR(30),
    mileage INTEGER NOT NULL,
    serial_number VARCHAR(50) UNIQUE,
    motor_number VARCHAR(50),
    plates VARCHAR(20),
    plates_holder VARCHAR(100),
    repuve VARCHAR(50),
    description TEXT,
    added_date DATE DEFAULT CURRENT_DATE
);

-- Subtabla: la agencia COMPRA la unidad.
CREATE TABLE vehicle_acquisitions (
    vehicle_folio INTEGER PRIMARY KEY REFERENCES vehicles(folio) ON DELETE CASCADE,
    seller_id INTEGER NOT NULL REFERENCES counterparties(id),
    invoice_type VARCHAR(50) NOT NULL CHECK (invoice_type IN ('Facturado', 'Autofactura')),
    invoice_number VARCHAR(50),
    invoice_issuer VARCHAR(150),
    invoice_file_path TEXT,
    purchase_price NUMERIC(12, 2) NOT NULL,
    payment_type VARCHAR(30) NOT NULL CHECK (payment_type IN ('Contado', 'Crédito')),
    payment_method VARCHAR(50) NOT NULL CHECK (payment_method IN ('Efectivo', 'Transferencia')),
    maintenance_cost NUMERIC(12, 2) DEFAULT 0.00,
    sale_price NUMERIC(12, 2) NOT NULL,
    observations TEXT,
    acquisition_date DATE NOT NULL DEFAULT CURRENT_DATE
);

-- Subtabla: la agencia VENDE por cuenta del dueño y cobra comisión.
-- Nota para quien escriba el INSERT: sale_price es GENERATED ALWAYS, así que
-- NO puede aparecer en la lista de columnas -- Postgres responde "cannot
-- insert a non-DEFAULT value into column". Usa RETURNING sale_price si
-- necesitas el valor calculado de vuelta.
CREATE TABLE vehicle_consignments (
    vehicle_folio INTEGER PRIMARY KEY REFERENCES vehicles(folio) ON DELETE CASCADE,
    owner_id INTEGER NOT NULL REFERENCES counterparties(id),
    invoice_type VARCHAR(50) NOT NULL CHECK (invoice_type IN ('Facturado REAL', 'No Facturado')),
    invoice_number VARCHAR(50),
    invoice_issuer VARCHAR(150),
    base_price NUMERIC(12, 2) NOT NULL,
    commission_rate NUMERIC(5, 2) NOT NULL,
    maintenance_cost NUMERIC(12, 2) DEFAULT 0.00,
    sale_price NUMERIC(12, 2) GENERATED ALWAYS AS (
        base_price + (base_price * (commission_rate / 100.00))
    ) STORED,
    observations TEXT,
    consignment_date DATE NOT NULL DEFAULT CURRENT_DATE
);

-- Especificaciones técnicas de la unidad (panel izquierdo del Paso 2).
CREATE TABLE vehicle_conditions (
    vehicle_folio INTEGER PRIMARY KEY REFERENCES vehicles(folio) ON DELETE CASCADE,
    fuel_type_id INTEGER REFERENCES fuel_type_cat(id),
    cylinders INTEGER NOT NULL,
    transmission VARCHAR(30) NOT NULL CHECK (transmission IN ('Automático', 'Manual')),
    interior_material VARCHAR(50),
    window_regulators VARCHAR(50) NOT NULL CHECK (window_regulators IN ('Manuales', 'Eléctricos tradicionales', 'Eléctricos inteligentes')),
    air_conditioning VARCHAR(50) NOT NULL CHECK (air_conditioning IN ('Automático', 'Manual')),
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Checklist de condición: un renglón SOLO por cada ítem que la unidad trae.
--
-- La presencia del renglón ES la respuesta a "¿lo trae?". No hay columna
-- is_checked: sería redundante, porque un renglón con is_checked = FALSE y la
-- ausencia del renglón dirían exactamente lo mismo, y tener las dos formas de
-- decirlo obliga a que toda consulta las contemple.
--
-- Por eso el checklist NO se lee desde esta tabla, sino desde el catálogo con
-- un LEFT JOIN, que es lo que reconstruye la pantalla completa:
--
--   SELECT c.id, c.category, c.element,
--          (i.id IS NOT NULL) AS is_present,
--          i.is_optimal, i.observations
--   FROM vehicle_conditions_cat c
--   LEFT JOIN vehicle_inspection i
--          ON i.element_id = c.id AND i.vehicle_folio = :folio
--   ORDER BY c.sort_order, c.id;
--
-- Los que faltan son los renglones con i.id IS NULL. Ojo al escribir filtros
-- sobre el lado derecho de un LEFT JOIN: la condición del vehículo va en el
-- ON y no en el WHERE, porque en el WHERE degrada el LEFT JOIN a INNER y
-- justo desaparecen las filas que se quieren detectar.
--
-- is_optimal solo tiene sentido cuando hay renglón: dice cómo salió el
-- elemento que sí se revisó.
--
-- element_id usa RESTRICT y no CASCADE a propósito: con CASCADE, borrar un
-- renglón del catálogo borraría en silencio esa línea de la inspección de
-- todos los vehículos históricos. Esta tabla es evidencia de lo que se
-- revisó, no configuración. Para retirar un ítem durante la fase de
-- definición, quítalo de la semilla y sube kSchemaVersion.
CREATE TABLE vehicle_inspection (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER NOT NULL REFERENCES vehicles(folio) ON DELETE CASCADE,
    element_id INTEGER NOT NULL REFERENCES vehicle_conditions_cat(id) ON DELETE RESTRICT,
    is_optimal BOOLEAN NOT NULL DEFAULT TRUE,
    observations TEXT,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_vehicle_element_inspection UNIQUE (vehicle_folio, element_id)
);

CREATE TABLE vehicle_images (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE CASCADE,
    file_path TEXT NOT NULL,
    is_primary BOOLEAN DEFAULT FALSE,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE vehicle_documents (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE CASCADE,
    document_type VARCHAR(50) NOT NULL,
    file_path TEXT,
    document_number VARCHAR(50),
    is_verified BOOLEAN DEFAULT FALSE,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_vehicle_doc UNIQUE(vehicle_folio, document_type)
);

CREATE TABLE vehicle_maintenance (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE CASCADE,
    maintenance_date DATE NOT NULL,
    maintenance_type_id INTEGER REFERENCES vehicle_maintenance_cat(id),
    cost NUMERIC(12, 2) NOT NULL,
    description TEXT
);

-- ===========================================================================
-- 4. VENTAS Y FINANCIAMIENTO
-- ===========================================================================

CREATE TABLE sales (
    folio SERIAL PRIMARY KEY,
    vehicle_folio INTEGER UNIQUE NOT NULL REFERENCES vehicles(folio),
    vendor_id INTEGER NOT NULL REFERENCES users(id),
    buyer_id INTEGER NOT NULL REFERENCES counterparties(id),
    payment_type VARCHAR(20) NOT NULL CHECK (payment_type IN ('Contado', 'Crédito')),
    payment_method VARCHAR(30) CHECK (payment_method IN ('Efectivo', 'Transferencia', 'Financiamiento')),
    subtotal NUMERIC(12, 2) NOT NULL,
    iva NUMERIC(12, 2) DEFAULT 16.00,
    total_amount NUMERIC(12, 2) NOT NULL,
    observations TEXT,
    status VARCHAR(20) CHECK (status IN ('Pago Completo', 'Por Cobrar', 'Atrasado', 'Cancelado')),
    sale_date DATE NOT NULL DEFAULT CURRENT_DATE
);

CREATE TABLE sale_financing (
    sale_folio INTEGER PRIMARY KEY REFERENCES sales(folio) ON DELETE CASCADE,
    down_payment NUMERIC(12, 2) NOT NULL,
    amount_to_finance NUMERIC(12, 2) NOT NULL,
    interest_rate NUMERIC(5, 2) NOT NULL,
    months_term INTEGER NOT NULL CHECK (months_term > 0),
    monthly_payment NUMERIC(12, 2) NOT NULL,
    final_calculated_price NUMERIC(12, 2) NOT NULL,
    aval_id INTEGER REFERENCES counterparties(id),
    status VARCHAR(20) DEFAULT 'Al Corriente' CHECK (status IN ('Al Corriente', 'Atrasado', 'Liquidado'))
);

CREATE TABLE sale_payments (
    id SERIAL PRIMARY KEY,
    sale_folio INTEGER NOT NULL REFERENCES sale_financing(sale_folio) ON DELETE CASCADE,
    installment_number INTEGER NOT NULL,
    scheduled_date DATE NOT NULL,
    expected_amount NUMERIC(12, 2) NOT NULL,
    paid_date DATE,
    paid_amount NUMERIC(12, 2) DEFAULT 0.00,
    capital_allocation NUMERIC(12, 2) DEFAULT 0.00,
    interest_allocation NUMERIC(12, 2) DEFAULT 0.00,
    remaining_balance NUMERIC(12, 2) NOT NULL,
    late_fee NUMERIC(12, 2) DEFAULT 0.00,
    status VARCHAR(20) DEFAULT 'Pendiente' CHECK (
        status IN ('Pendiente', 'Pagado', 'Parcial', 'Atrasado')
    ),
    is_waived BOOLEAN DEFAULT FALSE,
    payment_method VARCHAR(30),
    reference_number VARCHAR(100),
    notes TEXT,
    CONSTRAINT unique_installment_per_sale UNIQUE (sale_folio, installment_number)
);

-- ===========================================================================
-- 5. GASTOS OPERATIVOS Y CONFIGURACIÓN
-- ===========================================================================

CREATE TABLE operational_expenses (
    folio SERIAL PRIMARY KEY,
    expense_date DATE NOT NULL DEFAULT CURRENT_DATE,
    expense_id INTEGER NOT NULL REFERENCES operational_expense_cat(id),
    description TEXT,
    has_invoice BOOLEAN DEFAULT FALSE,
    invoice_number VARCHAR(50),
    subtotal NUMERIC(12, 2) NOT NULL,
    iva NUMERIC(12, 2) DEFAULT 0.00,
    total_amount NUMERIC(12, 2) NOT NULL
);

CREATE TABLE global_configurations (
    key_param VARCHAR(50) PRIMARY KEY,
    value_param NUMERIC(12, 2) NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ===========================================================================
-- 6. ÍNDICES QUE NO SALEN GRATIS DE UN PK/UNIQUE
-- ===========================================================================
-- PostgreSQL no indexa las columnas de una llave foránea automáticamente.
-- Sin este índice, el chequeo del ON DELETE RESTRICT al intentar borrar un
-- renglón del catálogo hace un seq scan de toda la tabla de inspecciones.
-- vehicle_folio no lo necesita: unique_vehicle_element_inspection ya crea un
-- btree con esa columna a la izquierda.

CREATE INDEX idx_vehicle_inspection_element ON vehicle_inspection(element_id);
