# Estabilización de transferencias

Objetivo: que instalación/transferencia sea el módulo más confiable del proyecto
antes de agregar funciones grandes nuevas.

## Estado honesto

No se considera 100% estable todavía. Está funcional y con pruebas reales exitosas,
pero XCI grandes, padding y cancelaciones largas siguen siendo zonas de riesgo.

## Casos que deben quedar claros para el usuario

| Caso | Resultado esperado |
|---|---|
| Archivo instalado nuevo | `OK`, indicar NCAs nuevas |
| Archivo ya instalado | `OK`, indicar contenido ya instalado/reutilizado |
| NCA existente con tamaño distinto | Fallo claro, no sobrescribir |
| XCI con `secure` fuera de rango | Fallo claro |
| XCI con padding/trailing bytes | Detectar en simulador/log y validar en hardware |
| Cancelación por usuario | Cancelado, conservar motivo |
| Desconexión o suspensión | Cancelado/fallido con detalle visible |
| MTP tamaño desconocido `0xFFFFFFFF` | Experimental, simulado antes de hardware |

## Herramientas de estabilización

- `scripts/functional_smoke.ps1`: pruebas + simulaciones + NRO.
- `tools/package_fixtures.py`: genera NSP/XCI sintéticos.
- `tools/mtp_stream_simulator.py`: simula envío MTP por chunks.
- `tools/install_log_analyzer.py`: resume logs reales de hardware.

## Próximos endurecimientos recomendados

1. Probar XCI grande real con log completo.
2. Analizar `install.log` con `install_log_analyzer.py`.
3. Convertir cada fallo real en fixture o prueba.
4. Mejorar UI para mostrar “ya instalado”, “fallido” y “cancelado” por archivo.
5. Evitar suspensión durante transferencias largas.
