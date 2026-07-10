# `switch_app`

Aplicación homebrew que corre en Nintendo Switch.

## Carpetas principales

- `source/`: implementación C/C++.
- `include/`: headers.
- `romfs/`: assets finales incluidos en el NRO.
- `data/`: assets fuente o material editable.
- `vendor/`: dependencias externas.
- `tests/`: pruebas C del núcleo.

## Compilación

Desde la raíz del proyecto:

```powershell
docker run --rm -v "${PWD}:/work" -w /work/switch_app `
  devkitpro/devkita64:latest bash -lc "source /opt/devkitpro/devkita64.sh && make clean && make -j2"
```

Resultado:

`dist/transferencia_switch.nro`

`switch_app/transferencia_switch.nro` también puede existir como salida intermedia
del Makefile, pero el archivo cómodo para copiar a la consola es el de `dist/`.
