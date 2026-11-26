#pragma once

/**
 * @brief The function type for every callback, timed or event.
 *
 * context is whatever was stored with the callback when it was set up (the same every
 * time). data is what the caller hands over for this one call: the time for timed
 * callbacks, whatever the event passes for events. data is only valid while the callback
 * runs: copy what you need, never keep the pointer.
 */
typedef void (*callback_t)(void *context, void *data);
