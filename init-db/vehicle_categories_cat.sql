-- 1. Categorías Principales (Nivel 1)
INSERT INTO vehicle_categories_cat (name, parent_id) VALUES
('Automóvil', NULL),
('Camioneta / SUV', NULL),
('Pickup / Comercial', NULL),
('Motocicleta', NULL)
ON CONFLICT DO NOTHING;

-- 2. Subtipos de Automóvil (Nivel 2)
INSERT INTO vehicle_categories_cat (name, parent_id) VALUES
('Sedán', (SELECT id FROM vehicle_categories_cat WHERE name = 'Automóvil' AND parent_id IS NULL)),
('Hatchback', (SELECT id FROM vehicle_categories_cat WHERE name = 'Automóvil' AND parent_id IS NULL)),
('Coupé', (SELECT id FROM vehicle_categories_cat WHERE name = 'Automóvil' AND parent_id IS NULL)),
('Convertible / Cabriolé', (SELECT id FROM vehicle_categories_cat WHERE name = 'Automóvil' AND parent_id IS NULL)),
('Monomovil / Minivan', (SELECT id FROM vehicle_categories_cat WHERE name = 'Automóvil' AND parent_id IS NULL))
ON CONFLICT DO NOTHING;

-- 3. Subtipos de Camioneta / SUV (Nivel 2)
INSERT INTO vehicle_categories_cat (name, parent_id) VALUES
('SUV Compacta', (SELECT id FROM vehicle_categories_cat WHERE name = 'Camioneta / SUV' AND parent_id IS NULL)),
('SUV Mediana', (SELECT id FROM vehicle_categories_cat WHERE name = 'Camioneta / SUV' AND parent_id IS NULL)),
('SUV Full-Size', (SELECT id FROM vehicle_categories_cat WHERE name = 'Camioneta / SUV' AND parent_id IS NULL)),
('Crossover', (SELECT id FROM vehicle_categories_cat WHERE name = 'Camioneta / SUV' AND parent_id IS NULL))
ON CONFLICT DO NOTHING;

-- 4. Subtipos de Pickup / Comercial (Nivel 2)
INSERT INTO vehicle_categories_cat (name, parent_id) VALUES
('Pickup Cabina Sencilla', (SELECT id FROM vehicle_categories_cat WHERE name = 'Pickup / Comercial' AND parent_id IS NULL)),
('Pickup Doble Cabina', (SELECT id FROM vehicle_categories_cat WHERE name = 'Pickup / Comercial' AND parent_id IS NULL)),
('Van de Carga / Pasajeros', (SELECT id FROM vehicle_categories_cat WHERE name = 'Pickup / Comercial' AND parent_id IS NULL)),
('Chasis Cabina / Estacas', (SELECT id FROM vehicle_categories_cat WHERE name = 'Pickup / Comercial' AND parent_id IS NULL))
ON CONFLICT DO NOTHING;

-- 5. Subtipos de Motocicleta (Nivel 2)
INSERT INTO vehicle_categories_cat (name, parent_id) VALUES
('Scooter / Trabajo', (SELECT id FROM vehicle_categories_cat WHERE name = 'Motocicleta' AND parent_id IS NULL)),
('Naked / Deportiva', (SELECT id FROM vehicle_categories_cat WHERE name = 'Motocicleta' AND parent_id IS NULL)),
('Cruiser / Chopper', (SELECT id FROM vehicle_categories_cat WHERE name = 'Motocicleta' AND parent_id IS NULL)),
('Doble Propósito / Adventure', (SELECT id FROM vehicle_categories_cat WHERE name = 'Motocicleta' AND parent_id IS NULL))
ON CONFLICT DO NOTHING;