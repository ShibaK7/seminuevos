INSERT INTO brands_cat (name) VALUES
-- Marcas Populares y de Gran Volumen (México)
('Nissan'),
('General Motors / Chevrolet'),
('Volkswagen'),
('Toyota'),
('Kia'),
('Ford'),
('Hyundai'),
('Honda'),
('Mazda'),
('Stellantis / RAM'),
('Stellantis / Jeep'),
('Stellantis / Dodge'),
('Stellantis / Chrysler'),
('Stellantis / FIAT'),
('SEAT'),
('Renault'),
('Suzuki'),

-- Marcas Chinas (Presencia Actual)
('MG Motors'),
('BYD'),
('Changan'),
('BAIC'),
('JAC Motors'),
('Omoda'),
('Jaecoo'),
('Geely'),
('GWM (Great Wall Motors)'),
('Chery / Chirey'),
('Jetour'),

-- Marcas Premium y de Lujo
('BMW'),
('Mercedes-Benz'),
('Audi'),
('Volvo'),
('Mini'),
('Acura'),
('Infiniti'),
('Lexus'),
('Lincoln'),
('Cadillac'),
('Porsche'),
('Alfa Romeo'),
('Tesla'),

-- Marcas Comerciales / Camiones Ligeros
('Isuzu'),
('Foton'),

-- Otras Marcas Tradicionales
('Peugeot'),
('Subaru'),
('Mitsubishi')
ON CONFLICT (name) DO NOTHING;