"""Simulador local de transferencia MTP por chunks.

No usa USB real ni instala contenidos. Su objetivo es reproducir mecánicamente los
casos que más nos importan antes de ir a hardware:

- tamaño conocido;
- tamaño desconocido (`0xFFFFFFFF`);
- cancelación a mitad;
- padding/trailing bytes;
- conteo de progreso por chunks.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

try:
    from tools.package_fixtures import inspect_package
except ModuleNotFoundError:  # Ejecución directa: python tools/mtp_stream_simulator.py
    from package_fixtures import inspect_package


UNKNOWN_MTP_SIZE = 0xFFFFFFFF


@dataclass(frozen=True)
class StreamSimulationResult:
    path: Path
    kind: str
    host_size: int
    advertised_size: int
    inferred_size: int
    chunk_size: int
    chunks: int
    transferred: int
    trailing_bytes: int
    cancelled: bool
    succeeded: bool
    detail: str


def format_report(result: StreamSimulationResult) -> str:
    status = "OK" if result.succeeded else "FALLO"
    advertised = (
        "desconocido/0xFFFFFFFF"
        if result.advertised_size == UNKNOWN_MTP_SIZE
        else str(result.advertised_size)
    )
    lines = [
        "Reporte simulacion MTP",
        "=======================",
        f"Estado: {status}",
        f"Archivo: {result.path}",
        f"Tipo: {result.kind}",
        f"Detalle: {result.detail}",
        f"Tamano host: {result.host_size}",
        f"Tamano anunciado: {advertised}",
        f"Tamano inferido: {result.inferred_size}",
        f"Transferido: {result.transferred}",
        f"Chunks: {result.chunks} x {result.chunk_size}",
        f"Bytes extra al final: {result.trailing_bytes}",
        f"Cancelado: {result.cancelled}",
    ]
    if result.trailing_bytes:
        lines.append("Accion sugerida: validar este caso en hardware antes de confiar en XCI grandes con padding.")
    elif not result.succeeded:
        lines.append("Accion sugerida: revisar tamano anunciado, cancelacion o estructura del paquete.")
    else:
        lines.append("Accion sugerida: apto para pasar a prueba de hardware si el NRO tambien compila.")
    return "\n".join(lines)


def simulate_mtp_send(
    path: Path,
    *,
    advertised_size: int | None = None,
    chunk_size: int = 16 * 1024,
    cancel_after: int | None = None,
) -> StreamSimulationResult:
    if chunk_size <= 0:
        raise ValueError("chunk_size debe ser positivo")

    info = inspect_package(path)
    host_size = path.stat().st_size
    effective_advertised = host_size if advertised_size is None else advertised_size
    unknown_size = effective_advertised == UNKNOWN_MTP_SIZE

    target_size = info.inferred_size if unknown_size else effective_advertised
    transferred = 0
    chunks = 0
    cancelled = False

    while transferred < target_size:
        next_size = min(chunk_size, target_size - transferred)
        if cancel_after is not None and transferred + next_size > cancel_after:
            transferred = cancel_after
            chunks += 1
            cancelled = True
            break
        transferred += next_size
        chunks += 1

    trailing = max(0, host_size - info.inferred_size)
    if cancelled:
        succeeded = False
        detail = "cancelado durante la recepcion"
    elif unknown_size and trailing:
        succeeded = False
        detail = "tamano inferido menor que archivo host; requiere validacion hardware"
    elif transferred != target_size:
        succeeded = False
        detail = "transferencia incompleta"
    elif not unknown_size and effective_advertised != info.inferred_size:
        succeeded = False
        detail = "tamano anunciado no coincide con tamano inferido"
    else:
        succeeded = True
        detail = "simulacion completada"

    return StreamSimulationResult(
        path=path,
        kind=info.kind,
        host_size=host_size,
        advertised_size=effective_advertised,
        inferred_size=info.inferred_size,
        chunk_size=chunk_size,
        chunks=chunks,
        transferred=transferred,
        trailing_bytes=trailing,
        cancelled=cancelled,
        succeeded=succeeded,
        detail=detail,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--unknown-size", action="store_true")
    parser.add_argument("--advertised-size", type=int)
    parser.add_argument("--chunk-size", type=int, default=16 * 1024)
    parser.add_argument("--cancel-after", type=int)
    parser.add_argument("--report", action="store_true")
    args = parser.parse_args()

    advertised = args.advertised_size
    if args.unknown_size:
        advertised = UNKNOWN_MTP_SIZE

    result = simulate_mtp_send(
        args.path,
        advertised_size=advertised,
        chunk_size=args.chunk_size,
        cancel_after=args.cancel_after,
    )
    if args.report:
        print(format_report(result))
    else:
        for key, value in result.__dict__.items():
            print(f"{key}={value}")
    return 0 if result.succeeded else 2


if __name__ == "__main__":
    raise SystemExit(main())
