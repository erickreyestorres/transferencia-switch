"""Cálculo de cobertura sin contar dos veces rangos repetidos."""

from dataclasses import dataclass, field


@dataclass(slots=True)
class RangeCoverage:
    total_size: int
    _ranges: list[tuple[int, int]] = field(default_factory=list)

    def __post_init__(self) -> None:
        if self.total_size < 0:
            raise ValueError("El tamaño total no puede ser negativo")

    def add(self, start: int, end: int) -> None:
        start = max(0, min(start, self.total_size))
        end = max(start, min(end, self.total_size))
        if start == end:
            return
        merged: list[tuple[int, int]] = []
        for current_start, current_end in sorted([*self._ranges, (start, end)]):
            if not merged or current_start > merged[-1][1]:
                merged.append((current_start, current_end))
            else:
                previous_start, previous_end = merged[-1]
                merged[-1] = (previous_start, max(previous_end, current_end))
        self._ranges = merged

    @property
    def transferred(self) -> int:
        return sum(end - start for start, end in self._ranges)

    @property
    def fraction(self) -> float:
        return 1.0 if self.total_size == 0 else min(1.0, self.transferred / self.total_size)

    @property
    def complete(self) -> bool:
        return self.transferred >= self.total_size
