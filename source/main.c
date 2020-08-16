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

#include "js110_statistics.h"
#include <stdio.h>
#include <signal.h>
#include <windows.h>


static int quit_ = 0;


void sigint_handler(int event) {
    (void) event;
    quit_ = 1;
}

void on_statistics(void * user_data, struct js110_statistics_s * statistics) {
    (void) user_data;
    printf("> %u: %llu samples, %f A, %f V, %f, W, %f C, %f J\n",
           statistics->serial_number,
           statistics->samples_total,
           statistics->current_mean,
           statistics->voltage_mean,
           statistics->power_mean,
           statistics->charge,
           statistics->energy);
}


int main(int argc, char * argv[]) {
    (void) argc;
    (void) argv;
    int rc;
    rc = js110_initialize(on_statistics, 0);
    if (rc) {
        printf("js110_initialize failed with %d\n", rc);
        return 1;
    }
    printf("Print statistics from all connected Joulescope instruments.\n");
    signal(SIGINT, sigint_handler);
    printf("Press CTRL-C to exit\n");
    while (!quit_) {
        Sleep(10);
    }

    js110_finalize();
    return 0;
}
