#include "sim/sim.h"
#include "event_queue.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct sim {
    event_queue_t *queue;
    sim_time_t now;
    sim_event_id_t last_event_id;
    bool running;
};

sim_status_t sim_create(sim_t **out) {
    if (!out) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    sim_t *result = malloc(sizeof(sim_t));
    if (!result) {
        return SIM_ERR_OUT_OF_MEMORY;
    }
    result->now = 0;
    result->last_event_id = 0;
    result->running = false;
    result->queue = event_queue_create();
    if (!result->queue) {
        free(result);
        return SIM_ERR_OUT_OF_MEMORY;
    }

    *out = result;
    return SIM_OK;
}

sim_status_t sim_destroy(sim_t *sim) {
    if (!sim) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (sim->running) {
        return SIM_ERR_REENTRANT;
    }
    while (!event_queue_is_empty(sim->queue)) {
        event_t to_clean;
        event_queue_pop(sim->queue, &to_clean);
        if (to_clean.user_context_destroy) {
            to_clean.user_context_destroy(to_clean.user_context);
        }
    }
    event_queue_destroy(sim->queue);
    free(sim);
    return SIM_OK;
}

sim_status_t sim_now(const sim_t *sim, sim_time_t *out) {
    if (!sim || !out) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    *out = sim->now;
    return SIM_OK;
}

sim_status_t sim_schedule_at(
    sim_t *sim,
    sim_time_t time,
    sim_callback_fn callback,
    void *user_context,
    sim_user_context_destroy_fn user_context_destroy,
    sim_event_id_t *out_id
) {
    if (!out_id) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    *out_id = 0;

    if (!sim || !callback) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (time < sim->now) {
        return SIM_ERR_TIME_IN_PAST;
    }
    if (sim->last_event_id == UINT64_MAX) {
        return SIM_ERR_ID_OVERFLOW;
    }
    event_t new_event = {
        .id = sim->last_event_id + 1,
        .time = time,
        .callback = callback,
        .user_context = user_context,
        .user_context_destroy = user_context_destroy
    };
    if (!event_queue_insert(sim->queue, &new_event)) {
        return SIM_ERR_OUT_OF_MEMORY;
    }

    ++sim->last_event_id;
    *out_id = sim->last_event_id;

    return SIM_OK;
}

sim_status_t sim_schedule_after(
    sim_t *sim,
    sim_time_t delay,
    sim_callback_fn callback,
    void *user_context,
    sim_user_context_destroy_fn user_context_destroy,
    sim_event_id_t *out_id
) {
    if (!out_id) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    *out_id = 0;

    if (!sim || !callback) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (delay > UINT64_MAX - sim->now) {
        return SIM_ERR_TIME_OVERFLOW;
    }
    if (sim->last_event_id == UINT64_MAX) {
        return SIM_ERR_ID_OVERFLOW;
    }
    event_t new_event = {
        .id = sim->last_event_id + 1,
        .time = sim->now + delay,
        .callback = callback,
        .user_context = user_context,
        .user_context_destroy = user_context_destroy
    };
    if (!event_queue_insert(sim->queue, &new_event)) {
        return SIM_ERR_OUT_OF_MEMORY;
    }

    ++sim->last_event_id;
    *out_id = sim->last_event_id;

    return SIM_OK;
}

sim_status_t sim_cancel(sim_t *sim, sim_event_id_t event_id) {
    if (!sim || !event_id) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    event_t to_clean;
    if (!event_queue_remove(sim->queue, event_id, &to_clean)) {
        return SIM_ERR_NOT_FOUND;
    }
    if (to_clean.user_context_destroy) {
        to_clean.user_context_destroy(to_clean.user_context);
    }
    return SIM_OK;
}

sim_status_t sim_run(sim_t *sim) {
    if (!sim) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (sim->running) {
        return SIM_ERR_REENTRANT;
    }
    sim->running = true;
    while (!event_queue_is_empty(sim->queue)) {
        event_t to_execute;
        event_queue_pop(sim->queue, &to_execute);
        sim->now = to_execute.time;
        sim_status_t status =
            to_execute.callback(sim, to_execute.id, sim->now, to_execute.user_context);
        if (to_execute.user_context_destroy) {
            to_execute.user_context_destroy(to_execute.user_context);
        }
        if (status != SIM_OK) {
            sim->running = false;
            return SIM_ERR_CALLBACK;
        }
    }
    sim->running = false;
    return SIM_OK;
}

sim_status_t sim_run_until(sim_t *sim, sim_time_t until) {
    if (!sim) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (until < sim->now) {
        return SIM_ERR_TIME_IN_PAST;
    }
    if (sim->running) {
        return SIM_ERR_REENTRANT;
    }
    sim->running = true;
    while (true) {
        event_t to_execute;
        if (!event_queue_peek(sim->queue, &to_execute) || to_execute.time > until) {
            sim->now = until;
            break;
        }
        event_queue_pop(sim->queue, &to_execute);
        sim->now = to_execute.time;
        sim_status_t status =
            to_execute.callback(sim, to_execute.id, sim->now, to_execute.user_context);
        if (to_execute.user_context_destroy) {
            to_execute.user_context_destroy(to_execute.user_context);
        }
        if (status != SIM_OK) {
            sim->running = false;
            return SIM_ERR_CALLBACK;
        }
    }
    sim->running = false;
    return SIM_OK;
}

const char *sim_status_string(sim_status_t status) {
    switch (status) {
    case SIM_OK:
        return "Success";
    case SIM_ERR_INVALID_ARGUMENT:
        return "Invalid argument";
    case SIM_ERR_OUT_OF_MEMORY:
        return "Out of memory";
    case SIM_ERR_NOT_FOUND:
        return "Not found";
    case SIM_ERR_TIME_IN_PAST:
        return "Event time is in the past";
    case SIM_ERR_TIME_OVERFLOW:
        return "Simulation time overflow";
    case SIM_ERR_ID_OVERFLOW:
        return "Event ID overflow";
    case SIM_ERR_CALLBACK:
        return "Event callback failed";
    case SIM_ERR_REENTRANT:
        return "Reentrant operation";
    default:
        return "Unknown status";
    }
}
