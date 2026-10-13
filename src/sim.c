#include "sim/sim.h"

struct sim_t {};

struct sim_time_t {};

sim_status_t sim_create(sim_t **out) { return SIM_OK; }

sim_status_t sim_destroy(sim_t *sim) { return SIM_OK; }

sim_status_t sim_now(const sim_t *sim, sim_time_t *out) { return SIM_OK; }

sim_status_t sim_schedule_at(
    sim_t *sim,
    sim_time_t time,
    sim_callback_fn callback,
    void *user_context,
    sim_user_context_destroy_fn user_context_destroy,
    sim_event_id_t *out_id
) {
    return SIM_OK;
}

sim_status_t sim_schedule_after(
    sim_t *sim,
    sim_time_t delay,
    sim_callback_fn callback,
    void *user_context,
    sim_user_context_destroy_fn user_contexuser_context,
    sim_event_id_t *out_id
) {
    return SIM_OK;
}

sim_status_t sim_cancel(sim_t *sim, sim_event_id_t event_id) { return SIM_OK; }

sim_status_t sim_run(sim_t *sim) { return SIM_OK; }

sim_status_t sim_run_until(sim_t *sim, sim_time_t until) { return SIM_OK; }

const char *sim_status_string(sim_status_t status) { return "\0"; };
