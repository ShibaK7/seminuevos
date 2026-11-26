-- ===========================================================================
-- CONFIGURACIÓN GLOBAL: parámetros del negocio (Configuraciones, US-07.1)
-- ===========================================================================
-- Valores con que arranca una base nueva. Desde la US-07.1 el Administrador
-- los cambia en la pantalla Configuraciones, y cada uno guarda la fecha de su
-- último cambio en updated_at.
--
-- UMA_DIARIA: la UMA vigente, para el tope de pago en efectivo de la
-- LFPIORPI (art. 32, fr. II). Sin esta fila la app no conoce la UMA y
-- rechaza los pagos en efectivo.
--   UMA diaria 2026 = $117.31 (INEGI, publicada en el DOF el 9-ene-2026,
--   vigente desde el 1-feb-2026). Cambia CADA AÑO: el INEGI publica la nueva
--   en enero y rige desde el 1 de febrero. Se actualiza en Configuraciones.
--
-- TOPE_EFECTIVO_UMA: cuántas UMA son el tope de pago en efectivo. La ley dice
--   3,210 (3,210 x 117.31 = $376,565.10). El Administrador puede fijar uno más
--   estricto, nunca uno mayor: la app rechaza un valor arriba de 3,210 y, si
--   lo encontrara aquí, usaría el legal.
--
-- SALDO_BANCO, SALDO_CAJA_CHICA, SALDO_CAJA_FUERTE: saldos en pesos que
--   captura el Administrador y lee Finanzas.
--
-- Docker solo corre init-db/ cuando el volumen está vacío. En una base que ya
-- existe se puede correr a mano para agregar las llaves que falten (usuario y
-- base son los de .env; en Git Bash, anteponer MSYS_NO_PATHCONV=1):
--
--   docker exec seminuevos_postgres psql -U seminuevos_app -d seminuevos -f /docker-entrypoint-initdb.d/90_global_configurations.sql
--
-- Es ON CONFLICT DO NOTHING a propósito: correrlo de nuevo nunca pisa lo que
-- el Administrador capturó. Antes de la US-07.1 la UMA se actualizaba
-- corriendo este archivo; ahora eso desharía su cambio.
--
-- Ojo: una sola llave guarda solo la UMA vigente. La ley toma la del día del
-- pago, así que una operación anterior al cambio de febrero se evaluaría con
-- el valor nuevo. Si hiciera falta precisión histórica, habría que guardar la
-- UMA por periodo de vigencia.
-- ===========================================================================

INSERT INTO global_configurations (key_param, value_param)
VALUES ('UMA_DIARIA', 117.31),
       ('TOPE_EFECTIVO_UMA', 3210),
       ('SALDO_BANCO', 0),
       ('SALDO_CAJA_CHICA', 0),
       ('SALDO_CAJA_FUERTE', 0)
ON CONFLICT (key_param) DO NOTHING;
