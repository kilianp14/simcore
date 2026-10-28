from cffi import FFI

ffibuilder = FFI()


ffibuilder.cdef(
    """
    typedef struct sim sim_t;
    typedef uint64_t sim_time_t;
    typedef uint64_t sim_event_id_t;
    typedef int32_t sim_status_t;

    enum {
        SIM_OK = 0,
        SIM_ERR_INVALID_ARGUMENT = 1,
        SIM_ERR_OUT_OF_MEMORY = 2,
        SIM_ERR_NOT_FOUND = 3,
        SIM_ERR_TIME_IN_PAST = 4,
        SIM_ERR_TIME_OVERFLOW = 5,
        SIM_ERR_ID_OVERFLOW = 6,
        SIM_ERR_CALLBACK = 7,
        SIM_ERR_REENTRANT = 8
    };

    typedef sim_status_t (*sim_callback_fn)(
        sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context
    );
    typedef void (*sim_user_context_destroy_fn)(void *user_context);

    sim_status_t sim_create(sim_t **out);
    sim_status_t sim_destroy(sim_t *sim);
    sim_status_t sim_now(const sim_t *sim, sim_time_t *out);
    sim_status_t sim_schedule_at(
        sim_t *sim,
        sim_time_t time,
        sim_callback_fn callback,
        void *user_context,
        sim_user_context_destroy_fn user_context_destroy,
        sim_event_id_t *out_id
    );
    sim_status_t sim_schedule_after(
        sim_t *sim,
        sim_time_t delay,
        sim_callback_fn callback,
        void *user_context,
        sim_user_context_destroy_fn user_context_destroy,
        sim_event_id_t *out_id
    );
    sim_status_t sim_cancel(sim_t *sim, sim_event_id_t event_id);
    sim_status_t sim_run(sim_t *sim);
    sim_status_t sim_run_until(sim_t *sim, sim_time_t until);
    const char *sim_status_string(sim_status_t status);

    extern "Python" sim_status_t _sim_python_event_callback(
        sim_t *, sim_event_id_t, sim_time_t, void *
    );
    extern "Python" void _sim_python_context_destroy(void *);
    """
)

ffibuilder.set_source(
    "simcore._simcorecffi",
    '#include "sim/sim.h"',
    include_dirs=["../include", "../src/internal"],
    sources=[
        "../src/sim.c",
        "../src/event_queue.c",
    ],
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
