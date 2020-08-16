/*
 * Copyright 2020 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef JS110_H__
#define JS110_H__

#include <stdint.h>


#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief A single statistics update.
 */
struct js110_statistics_s {
    /// The source JS110 serial number.
    uint32_t serial_number;

    /// The number of samples in this window.  0 for no update.
    int32_t samples_this;
    /// The number of samples in each update.
    int32_t samples_per_update;
    /// The number of samples per second.
    int32_t samples_per_second;

    /// The total number of samples used to compute charge and energy
    int64_t samples_total;
    /// The total charge over sample_total samples.
    double charge;
    /// The total energy over sample_totals samples.
    double energy;

    /// The average current over samples_this samples.
    double current_mean;
    /// The minimum current over samples_this samples.
    double current_min;
    /// The maximum current over samples_this samples.
    double current_max;

    /// The average voltage over samples_this samples.
    double voltage_mean;
    /// The minimum voltage over samples_this samples.
    double voltage_min;
    /// The maximum voltage over samples_this samples.
    double voltage_max;

    /// The average power over samples_this samples.
    double power_mean;
    /// The minimum power over samples_this samples.
    double power_min;
    /// The maximum power over samples_this samples.
    double power_max;
};

/**
 * @brief The function called for each statistics update.
 *
 * @param user_data The arbitrary data.
 * @param statistics The statistics update structure.
 */
typedef void (*js110_statistics_cbk)(void * user_data, struct js110_statistics_s * statistics);

/**
 * @brief Initialize the JS110 statistics library.
 *
 * @param cbk_fn The function to call on statistics updates.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return 0 or error code.
 *
 * NOTE: Only one instance of this library should be run at a time on
 * a single host.  The library attempts to claim all connected JS110
 * devices, which also means that it does not play nicely with any
 * other Joulescope-enabled applications, including the Joulescope UI.
 */
int js110_initialize(js110_statistics_cbk cbk_fn, void * cbk_user_data);

/**
 * @brief Finalize the JS110 library.
 *
 * @return 0 or error code.
 */
int js110_finalize(void);


#if defined(__cplusplus)
}
#endif

#endif  /* JS110_H__ */
