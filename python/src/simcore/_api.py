"""Public Python API for simcore."""

from collections.abc import Callable
from types import TracebackType
from typing import Literal, Self, TypeAlias


EventCallback: TypeAlias = Callable[["Simulator", int, int], None]


class Simulator:
    """A deterministic, discrete-event simulator."""

    def __init__(self) -> None:
        """Create a new simulator at simulation time zero."""
        raise NotImplementedError

    @property
    def now(self) -> int:
        """Return the current simulation time."""
        raise NotImplementedError

    def schedule_at(
        self,
        time: int,
        callback: EventCallback,
    ) -> int:
        """Schedule a callback at an absolute simulation time.

        Return the assigned event ID.
        """
        raise NotImplementedError

    def schedule_after(
        self,
        delay: int,
        callback: EventCallback,
    ) -> int:
        """Schedule a callback relative to the current simulation time.

        Return the assigned event ID.
        """
        raise NotImplementedError

    def cancel(self, event_id: int) -> None:
        """Cancel a pending event."""
        raise NotImplementedError

    def run(self) -> None:
        """Process events until the event queue is empty."""
        raise NotImplementedError

    def run_until(self, until: int) -> None:
        """Process all events scheduled at or before `until`."""
        raise NotImplementedError

    def close(self) -> None:
        """Destroy the simulator and release its resources."""
        raise NotImplementedError

    def __enter__(self) -> Self:
        """Enter a context-managed simulator scope."""
        raise NotImplementedError

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> Literal[False]:
        """Close the simulator without suppressing exceptions."""
        raise NotImplementedError
