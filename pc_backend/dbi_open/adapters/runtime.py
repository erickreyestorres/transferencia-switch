"""Adaptadores de eventos, cancelación, reloj y escritura."""

from collections.abc import Callable
import threading
import time
from typing import Any


class CallbackEventPublisher:
    def __init__(self, callback: Callable[[dict[str, Any]], None]) -> None:
        self.callback = callback

    def publish(self, event_type: str, **payload: Any) -> None:
        self.callback({"type": event_type, **payload})


class EventCancellationToken:
    def __init__(self, event: threading.Event) -> None:
        self.event = event

    @property
    def is_cancelled(self) -> bool:
        return self.event.is_set()


class SystemClock:
    def monotonic(self) -> float:
        return time.monotonic()


class EndpointWriter:
    def __init__(self, endpoint: Any) -> None:
        self.endpoint = endpoint

    def write(self, payload: bytes) -> int:
        return int(self.endpoint.write(payload, timeout=0))
