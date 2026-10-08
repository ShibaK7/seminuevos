INSERT INTO fuel_type_cat (name) VALUES
('Gasolina'),
('Diésel'),
('Híbrido (Gasolina/Eléctrico)'),
('Híbrido Enchufable (PHEV)'),
('Eléctrico (BEV)'),
('Gas Natural Vehicular (GNV)'),
('Gas Licuado de Petróleo (GLP)')
ON CONFLICT (name) DO NOTHING;