
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "ruleng.h"
#include "utils.h"

#define RULENG_DEFAULT_UBUS_PATH "/var/run/ubus.sock"
#define RULENG_DEFAULT_RULES_PATH "rules"

static void
ruleng_usage(char *n)
{
    fprintf(stdout,
        "\nUsage: %s: <options>\n"
        "Options:\n"
        "  -s <socket> path to ubus socket [" RULENG_DEFAULT_UBUS_PATH "]\n"
        "  -r <rules> path to the uci rules [" RULENG_DEFAULT_RULES_PATH "]\n"
        "  -h help\n\n"
        , n);
}

int
main(int argc, char **argv)
{
    char *sock = RULENG_DEFAULT_UBUS_PATH;
    char *rules = RULENG_DEFAULT_RULES_PATH;

    int c = -1;
    while((c = getopt(argc, argv,
                      "s:r:m:h"
               )) != -1) {
        switch (c) {
        case 'h':
            ruleng_usage(argv[0]);
            return EXIT_SUCCESS;
        case 's':
            sock = optarg;
            break;
        case 'r':
            rules = optarg;
            break;
        default:
            ruleng_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    int rc = EXIT_FAILURE;

    struct ruleng_ctx *ctx = NULL;
    if (RULENG_OK != ruleng_init(sock, rules, &ctx))
        goto exit;

    ruleng_uloop_run(ctx);

    ruleng_free(ctx);

    rc = EXIT_SUCCESS;
exit:
    return rc;
}
