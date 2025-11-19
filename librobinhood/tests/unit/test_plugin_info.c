#include <stdio.h>
#include <stdlib.h>
#include "robinhood/filters/core.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <backend-uri>\n", argv[0]);
        return 1;
    }

    struct rbh_backend_plugin_info info = get_backend_plugin_info(argv[1]);

    printf("Plugin: %s\n", info.plugin->plugin.name);
    printf("Extensions count: %d\n", info.extension_count);

    for (int i = 0; i < info.extension_count; ++i) {
        printf(" - Extension %d: %s\n", i, info.extensions[i]->name);
    }

    return 0;
}
