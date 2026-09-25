INSERT INTO vehicle_conditions_cat (category, element) VALUES
-- Exterior
('Exterior', 'Hojalatería'),
('Exterior', 'Pintura'),
('Exterior', 'Llantas'),
('Exterior', 'Molduras'),
('Exterior', 'Manijas'),
('Exterior', 'Parabrisas'),
('Exterior', 'Luces delanteras'),
('Exterior', 'Luces traseras'),
('Exterior', 'Espejo retrovisor'),
('Exterior', 'Espejos'),
('Exterior', 'Tapón de gasolina'),
('Exterior', 'Limpiadores'),
('Exterior', 'Antena'),
('Exterior', 'Tapones'),
('Exterior', 'Rines'),

-- Interior
('Interior', 'Asientos'),
('Interior', 'Alfombras'),
('Interior', 'Tapetes'),
('Interior', 'Instrumentación de tablero'),
('Interior', 'Radio'),
('Interior', 'Bocinas'),
('Interior', 'Ceniceros'),
('Interior', 'Encendedor'),
('Interior', 'Claxón'),

-- Mecánica
('Mecánica', 'Motor'),
('Mecánica', 'Frenos'),
('Mecánica', 'Suspensión'),
('Mecánica', 'Transmisión'),
('Mecánica', 'Dirección'),
('Mecánica', 'Escape'),

-- Seguridad
('Seguridad', 'Cinturón de seguridad'),
('Seguridad', 'Triángulo de seguridad'),
('Seguridad', 'Extintor'),

-- Accesorios
('Accesorios', 'Herramientas'),
('Accesorios', 'Llave de rueda'),
('Accesorios', 'Gato y maneral'),
('Accesorios', 'Llanta de refacción')
ON CONFLICT (category, element) DO NOTHING;