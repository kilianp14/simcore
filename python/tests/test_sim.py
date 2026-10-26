import gc
import weakref
import pytest

from simcore import Simulator

MAX_U64 = 2**64 - 1


@pytest.fixture
def calls():
    return []


@pytest.fixture
def callback(calls):
    def record(sim, event_id, now):
        calls.append((sim, event_id, now))

    return record


# Public API tests


def exercise_api(sim, callback, calls):
    assert sim.now == 0

    first = sim.schedule_at(5, callback)
    second = sim.schedule_after(3, callback)
    cancelled = sim.schedule_at(4, callback)

    assert first == 1
    assert second == 2
    assert cancelled == 3

    sim.cancel(cancelled)

    sim.run_until(3)

    assert sim.now == 3
    assert calls == [(sim, second, 3)]

    fourth = sim.schedule_after(2, callback)

    sim.run()

    assert sim.now == 5
    assert calls == [
        (sim, second, 3),
        (sim, first, 5),
        (sim, fourth, 5),
    ]


def test_simulator_explicit_close(callback, calls):
    sim = Simulator()
    exercise_api(sim, callback, calls)
    sim.close()

    # Closing should be idempotent
    sim.close()
    with pytest.raises(RuntimeError):
        _ = sim.now


def test_simulator_context_manager(callback, calls):
    with Simulator() as sim:
        exercise_api(sim, callback, calls)


# Exception validation tests


@pytest.mark.parametrize(
    ("method", "args", "exception"),
    [
        ("schedule_at", ("1", lambda *_: None)),
        ("schedule_at", (0, None)),
        ("schedule_after", ("1", lambda *_: None)),
        ("schedule_after", (0, None)),
        ("cancel", ("1",)),
        ("run_until", ("1",)),
    ],
)
def test_argument_type_validation(method, args, exception):
    with Simulator() as sim:
        with pytest.raises(TypeError):
            getattr(sim, method)(*args)


@pytest.mark.parametrize(
    ("method", "args", "advance_first"),
    [
        # Negative absolute time and delay.
        ("schedule_at", (-1, lambda *_: None), False),
        ("schedule_after", (-1, lambda *_: None), False),
        # Scheduling in the past.
        ("schedule_at", (0, lambda *_: None), True),
        # Non-positive event IDs.
        ("cancel", (0,), False),
        ("cancel", (-1,), False),
        # Negative time and run_until in the past.
        ("run_until", (-1,), False),
        ("run_until", (0,), True),
    ],
)
def test_argument_value_validation(method, args, advance_first):
    with Simulator() as sim:
        if advance_first:
            sim.run_until(1)

        with pytest.raises(ValueError):
            getattr(sim, method)(*args)


@pytest.mark.parametrize(
    ("method", "args", "advance_to_max"),
    [
        ("schedule_at", (MAX_U64 + 1, lambda *_: None), False),
        ("schedule_after", (MAX_U64 + 1, lambda *_: None), False),
        ("schedule_after", (1, lambda *_: None), True),
        ("cancel", (MAX_U64 + 1,), False),
        ("run_until", (MAX_U64 + 1,), False),
    ],
)
def test_argument_overflow_validation(method, args, advance_to_max):
    with Simulator() as sim:
        if advance_to_max:
            sim.run_until(MAX_U64)

        with pytest.raises(OverflowError):
            getattr(sim, method)(*args)


def test_cancel_unknown_event_raises_key_error():
    with Simulator() as sim:
        with pytest.raises(KeyError):
            sim.cancel(1)


@pytest.mark.parametrize(
    ("method", "args"),
    [
        ("now", ()),
        ("schedule_at", (0, lambda *_: None)),
        ("schedule_after", (0, lambda *_: None)),
        ("cancel", (1,)),
        ("run", ()),
        ("run_until", (0,)),
        ("__enter__", ()),
    ],
)
def test_operations_on_closed_simulator_raise_runtime_error(method, args):
    sim = Simulator()
    sim.close()

    with pytest.raises(RuntimeError):
        getattr(sim, method)(*args)


@pytest.mark.parametrize(
    ("method", "args"),
    [
        ("run", ()),
        ("run_until", (0,)),
        ("close", ()),
        ("__exit__", (None, None, None)),
    ],
)
def test_operations_during_active_run_raise_runtime_error(method, args):
    with Simulator() as sim:

        def callback(owner, event_id, now):
            with pytest.raises(RuntimeError):
                getattr(owner, method)(*args)

        sim.schedule_at(0, callback)
        sim.run()

        # A failed reentrant operation must not invalidate the simulator.
        sim.schedule_at(1, lambda *_: None)
        sim.run()
        assert sim.now == 1


def test_context_manager_does_not_suppress_exception():
    failure = ValueError("body failed")

    with pytest.raises(ValueError) as exc_info:
        with Simulator():
            raise failure

    assert exc_info.value is failure


# Callback behavior


def test_callback_can_schedule_and_cancel_events():
    with Simulator() as sim:
        calls = []
        event_ids = {}

        def first(owner, event_id, now):
            calls.append(("first", now))
            owner.cancel(event_ids["cancelled"])
            event_ids["new"] = owner.schedule_at(
                now, lambda _sim, _id, t: calls.append(("new", t + 2))
            )

        def cancelled(owner, event_id, now):
            calls.append(("cancelled", now))

        event_ids["first"] = sim.schedule_at(1, first)
        event_ids["cancelled"] = sim.schedule_at(2, cancelled)

        sim.run()

        assert calls == [("first", 1), ("new", 3)]


def test_callback_exception_propagates_and_simulator_recovers(callback, calls):
    with Simulator() as sim:
        failure = RuntimeError("callback failed")

        def failing_callback(owner, event_id, now):
            raise failure

        sim.schedule_at(1, failing_callback)
        succeeded_id = sim.schedule_at(2, callback)

        with pytest.raises(RuntimeError) as exc_info:
            sim.run()

        assert exc_info.value is failure
        assert sim.now == 1

        sim.run()

        assert calls == [(sim, succeeded_id, 2)]


# Lifetimes/ Ownership


@pytest.mark.parametrize("disposal", ["run", "cancel", "close"])
def test_callback_lifetime(disposal):
    # Re-define since fixtures have strong refs outside the function
    calls = []

    def callback(sim, event_id, now):
        calls.append(now)

    with Simulator() as sim:
        callback_ref = weakref.ref(callback)
        event_id = sim.schedule_at(1, callback)

        del callback
        gc.collect()

        assert callback_ref() is not None

        if disposal == "run":
            sim.run()
        elif disposal == "cancel":
            sim.cancel(event_id)
        else:
            sim.close()

        gc.collect()

        assert callback_ref() is None
        assert calls == ([1] if disposal == "run" else [])


def test_failed_schedule_does_not_retain_callback():
    with Simulator() as sim:

        def callback(sim, event_id, now):
            pass

        callback_ref = weakref.ref(callback)

        with pytest.raises(ValueError):
            sim.schedule_at(-1, callback)

        del callback
        gc.collect()

        assert callback_ref() is None
