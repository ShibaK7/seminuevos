INSERT INTO vehicle_maintenance_cat (name) VALUES
('Mecánica'),
('Eléctrico'),
('Suspensión'),
('Hojalatería y Pintura'),
('Llantas'),
('Accesorios'),
('Vestiduras')
ON CONFLICT (name) DO NOTHING;