#include <json-c/json.h>

#include "../config.h"
#include "interface.h"
#include "vwire.h"

static int
vwire_json_load(interface_config_t *interface_config)
{
    const char *file = "vwire.json";
    char f[MAX_FILE_PATH] = {0};
    long fsize;
    char *js = NULL;
    json_object *jr = NULL, *ja;
    int i, vwire_pairs_num;
    vwire_config_t *vwire_pairs_mem = NULL;
    int ret = 0;

    if (!interface_config) {
        printf("interface config is null\n");
        ret = -1;
        goto done;
    }

    sprintf(f, "%s/%s", CONFIG_PATH, file);

    FILE *fd = fopen(f, "r");
    if (!fd) {
        printf("open file %s failed\n", f);
        ret = -1;
        goto done;
    }

    fseek(fd, 0, SEEK_END);
    fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    
    js = (char *)malloc(fsize + 1);
    fread(js, 1, fsize, fd);
    js[fsize] = '\0';
    fclose(fd);

    jr = json_tokener_parse(js);
    if (!jr) {
        printf("can't find json root in file %s\n", f);
        ret = -1;
        goto done;
    }

    if (json_object_object_get_ex(jr, "vwire_pairs", &ja) == 0) {
        printf("can't find vwire pair array in file %s\n", f);
        ret = -1;
        goto done;
    }

    vwire_pairs_num = json_object_array_length(ja);
    vwire_pairs_mem = (vwire_config_t *)malloc(sizeof(vwire_config_t) * vwire_pairs_num);

    for (i = 0; i < vwire_pairs_num; i++) {
        json_object *jo, *jv;
        vwire_config_t *vwire_config = vwire_pairs_mem + i;
        jo = json_object_array_get_idx(ja, i);

        if (!json_object_object_get_ex(jo, "id", &jv)) {
            printf("vwire pair id not found\n");
            ret = -1;
            goto done;
        }
        vwire_config->id = atoi(json_object_get_string(jv));

        if (!json_object_object_get_ex(jo, "port1", &jv)) {
            printf("port1 not found\n");
            ret = -1;
            goto done;
        }
        vwire_config->port1 = atoi(json_object_get_string(jv));

        if (!VWIRE_VALID_PORTID(vwire_config->port1)) {
            printf("port1 is invalid\n");
            ret = -1;
            goto done;
        }

        if (!json_object_object_get_ex(jo, "port2", &jv)) {
            printf("port2 not found\n");
            ret = -1;
            goto done;
        }
        vwire_config->port2 = atoi(json_object_get_string(jv));

        if (!VWIRE_VALID_PORTID(vwire_config->port2)) {
            printf("port2 is invalid\n");
            ret = -1;
            goto done;
        }

        printf("vwire pair %u port1 %u port2 %u\n",
            vwire_config->id, vwire_config->port1, vwire_config->port2);

        interface_config->vwire_pair_num ++;
    }

done:
    if (js) free(js);
    if (jr) json_object_put(jr);

    if (ret) {
        if (vwire_pairs_mem) {
            free(vwire_pairs_mem);
            interface_config->vwire_pair_num = 0;
        }
    } else {
        interface_config->vwire_pairs = vwire_pairs_mem;
    }

    return ret;
}

int vwire_init(__rte_unused void* cfg)
{
    config_t *config = (config_t *)cfg;
    interface_config_t *interface_config = (interface_config_t *)config->interface_config;
    
    int ret = vwire_json_load(interface_config);
    if (ret) {
        rte_exit(EXIT_FAILURE, "vwire json load error");
        return ret; 
    }

    return 0;
}

// file format utf-8
// ident using space