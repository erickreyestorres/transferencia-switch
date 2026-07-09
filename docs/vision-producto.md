# Visión del producto

## Objetivo

Crear una aplicación homebrew propia para Nintendo Switch que ejecute un responder
MTP interno y permita administrar archivos personales desde el cliente MTP normal de
Windows. DBI es la referencia funcional, no una dependencia ni una base de código que
se deba modificar.

## Experiencia en Windows

- La consola aparece como dispositivo portátil MTP.
- Windows usa su soporte WPD/MTP integrado; no se requiere un programa adicional.
- La navegación se organiza mediante almacenamientos y vistas publicados por la
  aplicación de Switch.
- No se intentará rediseñar el Explorador de Windows.

## Experiencia en la consola

- Interfaz gráfica propia, oscura y clara, sin apariencia de consola de depuración.
- Estado de conexión visible.
- Archivo actual, bytes transferidos, porcentaje, velocidad y tiempo estimado.
- Resumen acumulado de archivos correctos, fallidos, cancelados y omitidos.
- Motivo comprensible para cada fallo y posibilidad de reintento cuando sea seguro.
- Menú principal compuesto por tarjetas grandes con icono, nombre y estado.
- Control con cruceta y palancas de los Joy-Con desde la interfaz actual.
- Entrada táctil prevista como evolución: cada tarjeta será también una zona
  pulsable, sin acoplar el reconocimiento táctil al motor MTP.
- El diseño debe funcionar completamente con controles aunque el táctil no esté
  disponible o se decida incorporarlo en una entrega posterior.

## Reglas de integridad

- Una transferencia entrante se escribe primero en un archivo temporal.
- El archivo final solo se publica después de comprobar tamaño, sincronizar y completar
  correctamente el flujo recibido.
- Una cancelación o desconexión no debe reemplazar un archivo final válido.
- Las rutas se normalizan y se rechazan recorridos fuera del almacenamiento publicado.
- Las funciones de NAND y partidas comienzan en modo de lectura o respaldo y se amplían
  únicamente después de superar pruebas específicas.

## Entregas

1. Enumeración y lectura de la SD mediante MTP estándar.
2. Recepción controlada en `sdmc:/switch/transferencia-switch/inbox/`.
3. Cola, progreso y resumen de resultados en la pantalla de Switch.
4. Interfaz gráfica propia desacoplada del protocolo y del sistema de archivos.
5. SD completa con políticas de protección.
6. Vistas virtuales de álbum y respaldos de partidas propias.
7. Vistas avanzadas de solo lectura para diagnóstico y respaldo personal.

## Criterio inmediato de avance

La versión de lectura debe probarse en hardware real y cumplir lo siguiente antes de
habilitar escritura:

1. Windows muestra `Transferencia Switch` como dispositivo portátil.
2. Aparece `1: SD Card` y se pueden enumerar carpetas.
3. Un archivo puede copiarse de la SD al PC y coincide en tamaño y contenido.
4. Windows no puede crear, modificar ni borrar objetos en este modo.
5. Cerrar MTP con el botón indicado devuelve al menú sin bloquear la consola.
