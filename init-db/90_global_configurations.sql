-- ===========================================================================
-- CONFIGURACIÓN GLOBAL: valor diario de la UMA
-- ===========================================================================
-- La app lee UMA_DIARIA para el tope de la LFPIORPI (art. 32, fr. II): un
-- vehículo de 3,210 UMA o más no se puede pagar en efectivo. Sin esta fila
-- la app no conoce la UMA vigente.
--
-- UMA diaria 2026 = $117.31 (INEGI, publicada en el DOF el 9-ene-2026,
-- vigente desde el 1-feb-2026). El tope de efectivo para vehículos queda en
-- 3,210 x 117.31 = $376,565.10.
--
-- Se actualiza CADA AÑO: el INEGI publica la UMA nueva en enero y rige desde
-- el 1 de febrero. Docker solo corre init-db/ cuando el volumen está vacío,
-- así que en una base que ya existe este archivo se corre a mano (Docker ya
-- lo tiene montado dentro del contenedor; usuario y base son los de .env, y
-- en Git Bash hay que anteponer MSYS_NO_PATHCONV=1 para que no reescriba la
-- ruta):
--
--   docker exec seminuevos_postgres psql -U seminuevos_app -d seminuevos -f /docker-entrypoint-initdb.d/90_global_configurations.sql
--
-- Por eso es un upsert: al crear la base la tabla está vacía y el ON CONFLICT
-- no hace nada, pero al correrlo a mano reemplaza el valor del año anterior.
--
-- Ojo: una sola llave guarda solo la UMA vigente. La ley toma la del día del
-- pago, así que una operación anterior al cambio de febrero se evaluaría con
-- el valor nuevo. Si hiciera falta precisión histórica, habría que guardar la
-- UMA por periodo de vigencia.
-- ===========================================================================

INSERT INTO global_configurations (key_param, value_param)
VALUES ('UMA_DIARIA', 117.31)
ON CONFLICT (key_param) DO UPDATE
    SET value_param = EXCLUDED.value_param,
        updated_at = CURRENT_TIMESTAMP;
