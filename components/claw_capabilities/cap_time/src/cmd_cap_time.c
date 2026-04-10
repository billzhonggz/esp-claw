/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cmd_cap_time.h"

#include <stdio.h>
#include <stdlib.h>

#include "argtable3/argtable3.h"
#include "cap_time.h"
#include "esp_console.h"

static struct {
    struct arg_lit *now;
    struct arg_lit *set_timezone;
    struct arg_str *timezone;
    struct arg_end *end;
} time_args;

static int time_func(int argc, char **argv)
{
    char output[256] = {0};
    esp_err_t err;
    int nerrors = arg_parse(argc, argv, (void **)&time_args);
    int operation_count;

    if (nerrors != 0) {
        arg_print_errors(stderr, time_args.end, argv[0]);
        return 1;
    }

    operation_count = time_args.now->count + time_args.set_timezone->count;
    if (operation_count != 1) {
        printf("Exactly one operation must be specified\n");
        return 1;
    }

    if (time_args.set_timezone->count) {
        if (!time_args.timezone->count) {
            printf("'--set-timezone' requires '--timezone'\n");
            return 1;
        }

        err = cap_time_set_timezone(time_args.timezone->sval[0]);
        if (err != ESP_OK) {
            printf("time set-timezone failed: %s\n", esp_err_to_name(err));
            return 1;
        }

        printf("Timezone updated to %s\n", time_args.timezone->sval[0]);
        return 0;
    }

    err = cap_time_sync_now(output, sizeof(output));
    if (err != ESP_OK) {
        printf("time sync failed: %s\n", esp_err_to_name(err));
        return 1;
    }

    printf("%s\n", output);
    return 0;
}

void register_cap_time(void)
{
    time_args.now = arg_lit0(NULL, "now", "Fetch current network time and sync the local clock");
    time_args.set_timezone = arg_lit0(NULL, "set-timezone", "Set local timezone");
    time_args.timezone = arg_str0("t", "timezone", "<tz>", "Timezone string, for example CST-8");
    time_args.end = arg_end(4);

    const esp_console_cmd_t time_cmd = {
        .command = "time",
        .help = "Time operations.\n"
        "Examples:\n"
        " time --now\n"
        " time --set-timezone --timezone CST-8\n",
        .func = time_func,
        .argtable = &time_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&time_cmd));
}
