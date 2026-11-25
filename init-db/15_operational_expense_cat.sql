-- Tipos de gasto operativo de Finanzas (Gastos Generales). La descripción es
-- la que muestra "Agregar Gasto" al elegir el tipo y la que se guarda con el
-- gasto.
INSERT INTO operational_expense_cat (name, description) VALUES
('Renta', 'Pago de la renta del local de la agencia.'),
('Energía Eléctrica', 'Recibo de luz del local.'),
('Agua', 'Recibo de agua del local.'),
('Internet y Telefonía', 'Servicio de internet y líneas telefónicas de la agencia.'),
('Papelería', 'Material de oficina: hojas, tóner, carpetas y similares.'),
('Limpieza', 'Artículos y servicios de limpieza del local y de las unidades.'),
('Publicidad', 'Anuncios, redes sociales y material promocional.'),
('Gestoría', 'Trámites vehiculares: placas, verificaciones, bajas y cambios de propietario.'),
('Combustible', 'Gasolina para traslados y pruebas de manejo.'),
('Otros', 'Gastos operativos que no entran en otra categoría.')
ON CONFLICT (name) DO NOTHING;