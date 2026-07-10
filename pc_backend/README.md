# `pc_backend`

Backend USB privado heredado.

Hoy el programa principal usa MTP estándar, así que este backend no es obligatorio
para transferir desde Windows. Se conserva porque:

- ayuda a entender el diseño inicial;
- mantiene casos de prueba útiles;
- puede servir como herramienta de diagnóstico futura.

Si el proyecto se limpia más adelante, esta carpeta podría moverse a `legacy/`.

