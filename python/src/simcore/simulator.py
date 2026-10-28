"""Public Python API for simcore."""

from collections.abc import Callable
from dataclasses import dataclass
from types import TracebackType
from typing import Any, Literal, Self

from ._simcorecffi import ffi as _ffi  # type: ignore[import-not-found]
from ._simcorecffi import lib as _lib  # type: ignore[import-not-found]

type EventCallback = Callable[["Simulator", int, int], None]


class Simulator:
    """A deterministic, discrete-event simulator.

    The simulator owns a native C instance. Callbacks execute synchronously during run()
    or run_until(). The instance must be closed when no longer needed; context management
    provides automatic cleanup.
    A Simulator instance must not be accessed concurrently from multiple threads.
    """

    def __init__(self) -> None:
        """Create a new simulator at simulation time zero.

        Raises:
            MemoryError: If the native simulator cannot be allocated.
        """
        self._sim = _ffi.NULL
        self._closed = True
        self._event_handles: dict[object, Any] = {}
        self._callback_exception: BaseException | None = None

        out = _ffi.new("sim_t **")
        status = _lib.sim_create(out)

        if status == _lib.SIM_ERR_OUT_OF_MEMORY:
            raise MemoryError("Failed to allocate native simulator.")

        # Sanity check
        if status != _lib.SIM_OK:
            message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
            raise AssertionError(f"Unexpected sim_create() status: {message}")

        self._sim = out[0]
        self._closed = False

    @property
    def now(self) -> int:
        """Return the current simulation time.

        Returns:
            The current simulation time in ticks.

        Raises:
            RuntimeError: If the simulator is closed.
        """
        if self._closed:
            raise RuntimeError("Simulator closed.")

        out = _ffi.new("sim_time_t *")
        status = _lib.sim_now(self._sim, out)

        # Sanity check
        if status != _lib.SIM_OK:
            message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
            raise AssertionError(f"Unexpected sim_now() status: {message}")

        return int(out[0])

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
        if self._closed:
            raise RuntimeError("Simulator closed.")

        if not isinstance(time, int):
            raise TypeError("time must be an integer.")
        if not callable(callback):
            raise TypeError("callback must be callable.")
        if time < 0:
            raise ValueError("time must be non-negative.")
        if time > 2**64 - 1:
            raise OverflowError("time exceeds uint64.")

        token, handle = self._retain_event_context(callback)
        out = _ffi.new("sim_event_id_t *")

        status = _lib.sim_schedule_at(
            self._sim,
            time,
            _lib._sim_python_event_callback,
            handle,
            _lib._sim_python_context_destroy,
            out,
        )

        if status != _lib.SIM_OK:
            self._release_event_context(token)

            if status == _lib.SIM_ERR_OUT_OF_MEMORY:
                raise MemoryError("Failed to allocate event.")
            if status == _lib.SIM_ERR_TIME_IN_PAST:
                raise ValueError("Cannot schedule an event in the past.")
            if status == _lib.SIM_ERR_ID_OVERFLOW:
                raise OverflowError("Event ID space exhausted.")

            # Sanity check
            message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
            raise AssertionError(f"Unexpected sim_schedule_at() status: {message}")

        return int(out[0])

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
        if self._closed:
            raise RuntimeError("Simulator closed.")

        if not isinstance(delay, int):
            raise TypeError("delay must be an integer.")
        if not callable(callback):
            raise TypeError("callback must be callable.")
        if delay < 0:
            raise ValueError("delay must be non-negative.")
        if delay > 2**64 - 1:
            raise OverflowError("delay exceeds uint64.")

        token, handle = self._retain_event_context(callback)
        out = _ffi.new("sim_event_id_t *")

        status = _lib.sim_schedule_after(
            self._sim,
            delay,
            _lib._sim_python_event_callback,
            handle,
            _lib._sim_python_context_destroy,
            out,
        )

        if status != _lib.SIM_OK:
            self._release_event_context(token)

            if status == _lib.SIM_ERR_OUT_OF_MEMORY:
                raise MemoryError("Failed to allocate event.")
            if status == _lib.SIM_ERR_TIME_OVERFLOW:
                raise OverflowError("Resulting simulation time exceeds uint64.")
            if status == _lib.SIM_ERR_ID_OVERFLOW:
                raise OverflowError("Event ID space exhausted.")

            # Sanity check
            message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
            raise AssertionError(f"Unexpected sim_schedule_after() status: {message}")

        return int(out[0])

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
        if self._closed:
            raise RuntimeError("Simulator closed.")

        if not isinstance(event_id, int):
            raise TypeError("event_id must be an integer.")
        if event_id <= 0:
            raise ValueError("event_id must be positive.")
        if event_id > 2**64 - 1:
            raise OverflowError("event_id exceeds uint64.")

        status = _lib.sim_cancel(self._sim, event_id)

        if status == _lib.SIM_OK:
            return

        if status == _lib.SIM_ERR_NOT_FOUND:
            raise KeyError(event_id)
        if status == _lib.SIM_ERR_INVALID_ARGUMENT:
            raise AssertionError("sim_cancel() rejected a validated event ID.")

        # Sanity check
        message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
        raise AssertionError(f"Unexpected sim_cancel() status: {message}")

    def run(self) -> None:
        """Process events until the event queue is empty.

        Events scheduled by callbacks are processed during the same run.

        Raises:
            RuntimeError: If the simulator is closed or a run is already active.
            BaseException: If a callback raises, its original exception is re-raised
                after control returns from the native library.
        """
        if self._closed:
            raise RuntimeError("Simulator closed.")

        self._callback_exception = None

        status = _lib.sim_run(self._sim)

        if status == _lib.SIM_OK:
            return

        if status == _lib.SIM_ERR_REENTRANT:
            raise RuntimeError("Simulator is already running.")
        if status == _lib.SIM_ERR_CALLBACK:
            if self._callback_exception is None:
                raise AssertionError("Callback failed without recording an exception.")
            raise self._callback_exception

        # Sanity check
        message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
        raise AssertionError(f"Unexpected sim_run() status: {message}")

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
        if self._closed:
            raise RuntimeError("Simulator closed.")

        self._callback_exception = None

        if not isinstance(until, int):
            raise TypeError("until must be an integer.")
        if until < 0:
            raise ValueError("until must be non-negative.")
        if until > 2**64 - 1:
            raise OverflowError("until exceeds uint64.")

        status = _lib.sim_run_until(self._sim, until)

        if status == _lib.SIM_OK:
            return

        if status == _lib.SIM_ERR_REENTRANT:
            raise RuntimeError("Simulator is already running.")
        if status == _lib.SIM_ERR_TIME_IN_PAST:
            raise ValueError("until is earlier than the current simulation time.")
        if status == _lib.SIM_ERR_CALLBACK:
            if self._callback_exception is None:
                raise AssertionError("Callback failed without recording an exception.")
            raise self._callback_exception

        # Sanity check
        message = _ffi.string(_lib.sim_status_string(status)).decode("utf-8", errors="replace")
        raise AssertionError(f"Unexpected sim_run_until() status: {message}")

    def close(self) -> None:
        """Destroy the simulator and release its resources.

        Calling this method more than once is safe. Closing during an active run fails
        without invalidating the simulator.

        Raises:
            RuntimeError: If a run is active.
        """
        if not self._closed:
            status = _lib.sim_destroy(self._sim)
            if status == _lib.SIM_ERR_REENTRANT:
                raise RuntimeError("Simulator currently running.")

            # Sanity check
            if status != _lib.SIM_OK:
                message = _ffi.string(_lib.sim_status_string(status)).decode(
                    "utf-8", errors="replace"
                )
                raise AssertionError(f"Unexpected sim_create() status: {message}")

            self._closed = True
            self._sim = _ffi.NULL

    def __enter__(self) -> Self:
        """Enter a context-managed simulator scope.

        Returns:
            This simulator instance.

        Raises:
            RuntimeError: If the simulator is closed.
        """
        if self._closed:
            raise RuntimeError("Simulator closed.")
        return self

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
        self.close()
        return False

    def _retain_event_context(self, callback: EventCallback) -> tuple[object, Any]:
        token = object()
        record = _EventContext(
            simulator=self,
            callback=callback,
            token=token,
        )
        handle = _ffi.new_handle(record)

        # Keep the handle alive before its pointer is passed to C.
        self._event_handles[token] = handle

        return token, handle

    def _release_event_context(self, token: object) -> None:
        del self._event_handles[token]

    def _set_callback_exception(self, exc: BaseException) -> None:
        # Sanity: A run should stop after the first callback exception.
        if self._callback_exception is not None:
            raise AssertionError("A callback exception is already pending.")
        self._callback_exception = exc


@dataclass(slots=True)
class _EventContext:
    simulator: Simulator
    callback: EventCallback
    token: object


@_ffi.def_extern()
def _sim_python_event_callback(_sim, event_id, event_time, context):
    record: _EventContext = _ffi.from_handle(context)

    try:
        record.callback(
            record.simulator,
            int(event_id),
            int(event_time),
        )
    # Exceptions must not unwind through the C callback boundary.
    # Preserve the original exception and re-raise it from run()/run_until().
    except BaseException as exc:  # noqa: BLE001
        record.simulator._set_callback_exception(exc)
        return _lib.SIM_ERR_CALLBACK

    return _lib.SIM_OK


@_ffi.def_extern()
def _sim_python_context_destroy(context):
    record: _EventContext = _ffi.from_handle(context)
    record.simulator._release_event_context(record.token)
