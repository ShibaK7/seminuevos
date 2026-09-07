-- ===========================================================================
-- SCHEMA: seminuevos (concesionaria de autos)
-- ===========================================================================
-- Solo estructura -- los usuarios de prueba van en DevSeeder (C++), no aquí.
-- Idempotente a propósito: esta app la ejecuta SchemaInitializer cada vez
-- que arranca (no un script de init de Docker que solo corre una vez), así
-- que nunca debe usar DROP TABLE ni nada que borre datos existentes.
-- ===========================================================================

-- ===========================================================================
-- 1. LOOKUP & CATALOG TABLES (Must be created first for FK dependencies)
-- ===========================================================================

CREATE TABLE IF NOT EXISTS vehicle_categories_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    parent_id INTEGER REFERENCES vehicle_categories_cat(id) ON DELETE CASCADE,
    CONSTRAINT unique_name_per_parent UNIQUE NULLS NOT DISTINCT (name, parent_id)
);

CREATE TABLE IF NOT EXISTS brands_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS fuel_type_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS vehicle_maintenance_cat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL
);

-- ===========================================================================
-- 2. USER & COUNTERPARTY MANAGEMENT
-- ===========================================================================

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(100) NOT NULL,
    role VARCHAR(30) NOT NULL CHECK (role IN ('Administrador', 'Vendedor')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS counterparties (
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

CREATE TABLE IF NOT EXISTS counterparty_documents (
    id SERIAL PRIMARY KEY,
    counterparty_id INTEGER NOT NULL REFERENCES counterparties(id) ON DELETE CASCADE,
    document_type VARCHAR(50) NOT NULL,
    file_path TEXT NOT NULL,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_active_counterparty_doc UNIQUE (counterparty_id, document_type)
);

-- ===========================================================================
-- 3. INVENTORY & VEHICLE MANAGEMENT
-- ===========================================================================

-- Core Vehicle Table
CREATE TABLE IF NOT EXISTS vehicles (
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

-- Acquisition Flow
CREATE TABLE IF NOT EXISTS vehicle_acquisitions (
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

-- Consignment Flow
CREATE TABLE IF NOT EXISTS vehicle_consignments (
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

-- Vehicle Checklist & Mechanical Conditions
CREATE TABLE IF NOT EXISTS vehicle_conditions (
    vehicle_folio INTEGER PRIMARY KEY REFERENCES vehicles(folio) ON DELETE CASCADE,
    fuel_type_id INTEGER REFERENCES fuel_type_cat(id),
    cylinders INTEGER NOT NULL,
    transmission VARCHAR(30) NOT NULL CHECK (transmission IN ('Automático', 'Manual')),
    interior_material VARCHAR(50),
    window_regulators VARCHAR(50) NOT NULL CHECK (window_regulators IN ('Manuales', 'Eléctricos tradicionales', 'Eléctricos inteligentes')),
    air_conditioning VARCHAR(50) NOT NULL CHECK (air_conditioning IN ('Automático', 'Manual')),
    lights_front VARCHAR(20) DEFAULT 'Estado óptimo' CHECK (lights_front IN ('Estado óptimo', 'Con fallas')),
    lights_front_obs TEXT,
    lights_rear VARCHAR(20) DEFAULT 'Estado óptimo' CHECK (lights_rear IN ('Estado óptimo', 'Con fallas')),
    lights_rear_obs TEXT,
    tires VARCHAR(20) DEFAULT 'Estado óptimo' CHECK (tires IN ('Estado óptimo', 'Con fallas')),
    tires_obs TEXT,
    engine VARCHAR(20) DEFAULT 'Estado óptimo' CHECK (engine IN ('Estado óptimo', 'Con fallas')),
    engine_obs TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Checklist extendido de condición (accesorios + componentes), un renglón por
-- ítem inspeccionado. Reemplaza el enfoque de columnas fijas de
-- vehicle_conditions para los ~30+ ítems del checklist legacy (ver US-03.2) --
-- las columnas viejas de vehicle_conditions (lights_front/lights_rear/tires/
-- engine) se dejan sin usar, nunca se elimina una columna existente.
CREATE TABLE IF NOT EXISTS vehicle_condition_items (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER NOT NULL REFERENCES vehicles(folio) ON DELETE CASCADE,
    item_key VARCHAR(50) NOT NULL,
    item_group VARCHAR(30) NOT NULL,
    is_checked BOOLEAN DEFAULT TRUE,
    status VARCHAR(20) CHECK (status IN ('Estado óptimo', 'Con fallas')),
    observations TEXT,
    CONSTRAINT unique_vehicle_condition_item UNIQUE (vehicle_folio, item_key)
);

-- Images & Documents
CREATE TABLE IF NOT EXISTS vehicle_images (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE CASCADE,
    file_path TEXT NOT NULL,
    is_primary BOOLEAN DEFAULT FALSE,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS vehicle_documents (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE CASCADE,
    document_type VARCHAR(50) NOT NULL,
    file_path TEXT,
    document_number VARCHAR(50),
    is_verified BOOLEAN DEFAULT FALSE,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_vehicle_doc UNIQUE(vehicle_folio, document_type)
);

-- Maintenance Log
CREATE TABLE IF NOT EXISTS vehicle_maintenance (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER REFERENCES vehicles(folio) ON DELETE CASCADE,
    maintenance_date DATE NOT NULL,
    maintenance_type_id INTEGER REFERENCES vehicle_maintenance_cat(id),
    cost NUMERIC(12, 2) NOT NULL,
    description TEXT
);

-- ===========================================================================
-- 4. COMMERCIAL PIPELINE, SALES & FINANCING
-- ===========================================================================

-- Core Sales Header (Universal for Cash and Credit)
CREATE TABLE IF NOT EXISTS sales (
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

-- Credit Financing Details (1:1 Extension of sales)
CREATE TABLE IF NOT EXISTS sale_financing (
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

-- Amortization Schedule & Payment Collection Logs
CREATE TABLE IF NOT EXISTS sale_payments (
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
-- 5. OPERATIONAL EXPENSES & SYSTEM CONFIGURATION
-- ===========================================================================

CREATE TABLE IF NOT EXISTS operational_expenses (
    folio SERIAL PRIMARY KEY,
    expense_date DATE NOT NULL DEFAULT CURRENT_DATE,
    expense_type VARCHAR(50) NOT NULL,
    description TEXT,
    has_invoice BOOLEAN DEFAULT FALSE,
    invoice_number VARCHAR(50),
    subtotal NUMERIC(12, 2) NOT NULL,
    iva NUMERIC(12, 2) DEFAULT 0.00,
    total_amount NUMERIC(12, 2) NOT NULL
);

CREATE TABLE IF NOT EXISTS global_configurations (
    key_param VARCHAR(50) PRIMARY KEY,
    value_param NUMERIC(12, 2) NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
