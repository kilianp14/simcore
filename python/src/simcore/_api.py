"""Public Python API for simcore."""

from collections.abc import Callable
from types import TracebackType
from typing import Literal, Self

type EventCallback = Callable[["Simulator", int, int], None]


class Simulator:
    """A deterministic, discrete-event simulator.

    The simulator owns a native C instance. Callbacks execute synchronously during run()
    or run_until(). The instance must be closed when no longer needed; context management
    provides automatic cleanup.
    """

    def __init__(self) -> None:
        """Create a new simulator at simulation time zero.

        Raises:
            MemoryError: If the native simulator cannot be allocated.
        """
        raise NotImplementedError

    @property
    def now(self) -> int:
        """Return the current simulation time.

        Returns:
            The current simulation time in ticks.

        Raises:
            RuntimeError: If the simulator is closed.
        """
        raise NotImplementedError

    def schedule_at(self, time: int, callback: EventCallback) -> int:
        """Schedule a callback at an absolute simulation time.

        Events at the same time execute in ascending event-ID order starting at 1.

        Args:
            time: Absolute simulation time in ticks, from zero through ``2**64 - 1``.
            callback: Callable invoked with the simulator, event ID, and current time.

        Returns:
            The assigned event ID.

        Raises:
            TypeError: If ``time`` is not an integer or ``callback`` is not callable.
            ValueError: If ``time`` is negative or earlier than the current time.
            OverflowError: If ``time`` exceeds uint64 or event IDs are exhausted.
            MemoryError: If the native library cannot allocate the event.
            RuntimeError: If the simulator is closed.
        """
        raise NotImplementedError

    def schedule_after(self, delay: int, callback: EventCallback) -> int:
        """Schedule a callback relative to the current simulation time.

        A zero delay schedules the event at the current time.
        Events at the same time execute in ascending event-ID order starting at 1.

        Args:
            delay: Delay in ticks, from zero through ``2**64 - 1``.
            callback: Callable invoked with the simulator, event ID, and current time.

        Returns:
            The assigned event ID.

        Raises:
            TypeError: If ``delay`` is not an integer or ``callback`` is not callable.
            ValueError: If ``delay`` is negative.
            OverflowError: If the delay, resulting time, or event ID overflows uint64.
            MemoryError: If the native library cannot allocate the event.
            RuntimeError: If the simulator is closed.
        """
        raise NotImplementedError

    def cancel(self, event_id: int) -> None:
        """Cancel a pending event.

        Args:
            event_id: Positive event ID returned by a successful scheduling operation.

        Raises:
            TypeError: If ``event_id`` is not an integer.
            ValueError: If ``event_id`` is non-positive.
            OverflowError: If ``event_id`` exceeds uint64.
            KeyError: If the event ID does not identify a pending event.
            RuntimeError: If the simulator is closed.
        """
        raise NotImplementedError

    def run(self) -> None:
        """Process events until the event queue is empty.

        Events scheduled by callbacks are processed during the same run.

        Raises:
            RuntimeError: If the simulator is closed or a run is already active.
            BaseException: If a callback raises, its original exception is re-raised
                after control returns from the native library.
        """
        raise NotImplementedError

    def run_until(self, until: int) -> None:
        """Process all events scheduled at or before ``until``.

        On success, advance the simulation time to ``until``, even if no event occurs
        there. Events scheduled by callbacks are processed if their times are within
        the limit.

        Args:
            until: Inclusive simulation-time limit.

        Raises:
            TypeError: If ``until`` is not an integer.
            ValueError: If ``until`` is negative or earlier than the current time.
            OverflowError: If ``until`` exceeds uint64.
            RuntimeError: If the simulator is closed or a run is already active.
            BaseException: If a callback raises, its original exception is re-raised
                after control returns from the native library.
        """
        raise NotImplementedError

    def close(self) -> None:
        """Destroy the simulator and release its resources.

        Calling this method more than once is safe. Closing during an active run fails
        without invalidating the simulator.

        Raises:
            RuntimeError: If a run is active.
        """
        raise NotImplementedError

    def __enter__(self) -> Self:
        """Enter a context-managed simulator scope.

        Returns:
            This simulator instance.

        Raises:
            RuntimeError: If the simulator is closed.
        """
        raise NotImplementedError

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> Literal[False]:
        """Close the simulator without suppressing exceptions.

        Args:
            exc_type: Exception type raised in the context, or None if no exception occurred.
            exc_value: Exception instance raised in the context, or None.
            traceback: Traceback for the exception, or None.

        Returns:
            False, so any exception raised in the context propagates.

        Raises:
            RuntimeError: If closing fails because a run is active.
        """
        raise NotImplementedError
