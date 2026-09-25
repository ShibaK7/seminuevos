-- ===========================================================================
-- REFINED DATABASE SCHEMA: Class Table Inheritance Flow
-- ===========================================================================
DROP TABLE IF EXISTS sale_payments CASCADE;
DROP TABLE IF EXISTS sale_financing CASCADE;
DROP TABLE IF EXISTS sales CASCADE;
DROP TABLE IF EXISTS vehicle_maintenance CASCADE;
DROP TABLE IF EXISTS vehicle_documents CASCADE;
DROP TABLE IF EXISTS vehicle_images CASCADE;
DROP TABLE IF EXISTS vehicle_conditions_cat CASCADE;
DROP TABLE IF EXISTS vehicle_conditions CASCADE;
DROP TABLE IF EXISTS vehicle_inspection CASCADE;
DROP TABLE IF EXISTS vehicle_consignments CASCADE;
DROP TABLE IF EXISTS vehicle_acquisitions CASCADE;
DROP TABLE IF EXISTS vehicles CASCADE;
DROP TABLE IF EXISTS counterparty_documents CASCADE;
DROP TABLE IF EXISTS counterparties CASCADE;
DROP TABLE IF EXISTS users CASCADE;
DROP TABLE IF EXISTS vehicle_maintenance_cat CASCADE;
DROP TABLE IF EXISTS fuel_type_cat CASCADE;
DROP TABLE IF EXISTS brands_cat CASCADE;
DROP TABLE IF EXISTS vehicle_categories_cat CASCADE;
DROP TABLE IF EXISTS operational_expense_cat CASCADE;
DROP TABLE IF EXISTS operational_expenses CASCADE;
DROP TABLE IF EXISTS global_configurations CASCADE;

-- ===========================================================================
-- 1. LOOKUP & CATALOG TABLES (Must be created first for FK dependencies)
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

CREATE TABLE vehicle_conditions_cat (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    element VARCHAR(50),
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
-- 2. USER & COUNTERPARTY MANAGEMENT
-- ===========================================================================

CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(100) NOT NULL,
    role VARCHAR(30) NOT NULL CHECK (role IN ('Administrador', 'Vendedor')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

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
-- 3. INVENTORY & VEHICLE MANAGEMENT
-- ===========================================================================

-- Core Vehicle Table
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

-- Acquisition Flow
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

-- Consignment Flow
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

-- Vehicle Checklist & Mechanical Conditions
CREATE TABLE vehicle_conditions (
    vehicle_folio INTEGER PRIMARY KEY REFERENCES vehicles(folio) ON DELETE CASCADE,
    fuel_type_id INTEGER REFERENCES fuel_type_cat(id),
    cylinders INTEGER NOT NULL,
    transmission VARCHAR(30) NOT NULL CHECK (transmission IN ('Automático', 'Manual')),                 
    interior_material VARCHAR(50),
    window_regulators VARCHAR(50) NOT NULL CHECK (window_regulators IN ('Manuales', 'Eléctricos tradicionales', 'Eléctricos inteligentes')),
    air_conditioning VARCHAR(50) NOT NULL CHECK (air_conditioning IN ('Automático', 'Manual')), 
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE vehicle_inspection (
    id SERIAL PRIMARY KEY,
    vehicle_folio INTEGER NOT NULL REFERENCES vehicles(folio) ON DELETE CASCADE,
    element_id INTEGER NOT NULL REFERENCES vehicle_conditions_cat(id) ON DELETE CASCADE,
    is_optimal BOOLEAN DEFAULT TRUE,
    observations TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_vehicle_element_inspection UNIQUE (vehicle_folio, element_id)
);

-- Images & Documents
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

-- Maintenance Log
CREATE TABLE vehicle_maintenance (
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

-- Credit Financing Details (1:1 Extension of sales)
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

-- Amortization Schedule & Payment Collection Logs
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
-- 5. OPERATIONAL EXPENSES & SYSTEM CONFIGURATION
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