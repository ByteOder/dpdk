#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "json.h"

char *JS(const char *path, const char *name)
{
    char f[MAX_FILE_PATH] = {0};
    long fsize;
    char *js = NULL;

    if (!path || !name) return NULL;
    if (strlen(path) + strlen(name) > (MAX_FILE_PATH - 1)) return NULL;

    sprintf(f, "%s/%s", path, name);

    FILE *fd = fopen(f, "r");
    if (!fd) {
        perror("fopen");
        return NULL;
    }

    fseek(fd, 0, SEEK_END);
    fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    
    js = (char *)malloc(fsize + 1);
    fread(js, 1, fsize, fd);
    js[fsize] = '\0';
    fclose(fd);

    return js;
}

void JS_FREE(char *js)
{
    if (js) free(js);
}

json_object *JR(const char *js)
{
    if (js) return json_tokener_parse(js);
    return NULL;
}

void JR_FREE(json_object *jr)
{
    if (jr) json_object_put(jr);
}

int JA(json_object *jr, const char *tag, json_object **ja)
{
    if (!jr || !tag) return -1;
    if (!json_object_object_get_ex(jr, tag, ja)) return -1;
    return json_object_array_length(*ja);
}

json_object *JO(json_object *ja, int index)
{
    if (!ja) return NULL;
    return json_object_array_get_idx(ja, index);
}

json_object *JV(json_object *jo, const char *tag)
{
    json_object *jv;
    if (!jo || !tag) return NULL;
    if (!json_object_object_get_ex(jo, tag, &jv)) {
        return NULL;
    }
    return jv;
}

int JV_I(json_object *jv)
{
    return json_object_get_int(jv);
}

const char *JV_S(json_object *jv)
{
    return json_object_get_string(jv);
}

// file format utf-8
// ident using space