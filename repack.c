#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <cjson/cJSON.h>

char* processBytes(const char *arr, size_t length) {
    char *c = (char *)calloc(1, length + 1024);
    if (!c) return NULL;
    char hex[3] = {0};
    size_t ic = 0;

    for (size_t i = 0; i < length; i++) {
        if (arr[i] == '\\' && i + 3 < length && arr[i+1] == 'x') {
            hex[0] = tolower((unsigned char)arr[i+2]);
            hex[1] = tolower((unsigned char)arr[i+3]);
            c[ic] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else {
            c[ic] = arr[i];
        }
        ic++;
    }
    return c;
}

char* getPropertyChar(cJSON *prop) {
    cJSON *j_name = cJSON_GetObjectItemCaseSensitive(prop, "name");
    cJSON *j_length = cJSON_GetObjectItemCaseSensitive(prop, "length");
    cJSON *j_flags = cJSON_GetObjectItemCaseSensitive(prop, "flags");
    cJSON *j_value = cJSON_GetObjectItemCaseSensitive(prop, "value");

    int prop_length = j_length ? j_length->valueint : 0;
    uint32_t struct_size = 32 + sizeof(uint16_t) * 2; // 36 bytes
    uint32_t aligned_len = (prop_length + 0x3) & ~0x3;
    uint32_t size = struct_size + aligned_len;

    char *result = (char *)calloc(1, size);
    if (!result) return NULL;

    if (j_name && j_name->valuestring) {
        strncpy(result, j_name->valuestring, 31);
    }

    *(uint16_t*)&result[32] = (uint16_t)prop_length;
    *(uint16_t*)&result[34] = j_flags ? (uint16_t)j_flags->valueint : 0;

    if (j_value) {
        if (cJSON_IsNumber(j_value)) {
            uint64_t value = (uint64_t)j_value->valuedouble;
            switch (prop_length) {
                case 1:
                    *((uint8_t *)&result[struct_size]) = (uint8_t)value;
                    break;
                case 2:
                    *((uint16_t *)&result[struct_size]) = (uint16_t)value;
                    break;
                case 4:
                    *((uint32_t *)&result[struct_size]) = (uint32_t)value;
                    break;
                case 8:
                    *((uint64_t *)&result[struct_size]) = value;
                    break;
                default:
                    fprintf(stderr, "Unhandled int size %d\n", prop_length);
                    free(result);
                    abort();
            }
        } else if (cJSON_IsString(j_value) && j_value->valuestring && strlen(j_value->valuestring) > 0) {
            char *processed = processBytes(j_value->valuestring, strlen(j_value->valuestring) + 1);
            if (processed) {
                memcpy(&result[struct_size], processed, prop_length);
                free(processed);
            }
        }
    }
    return result;
}

char* getNodeChar(cJSON *node, uint32_t *full_size) {
    uint32_t n_properties = 0;
    uint32_t n_children = 0;

    cJSON *sub = NULL;
    cJSON_ArrayForEach(sub, node) {
        if (cJSON_IsObject(sub)) {
            n_properties++;
        } else if (cJSON_IsArray(sub)) {
            n_children++;
        }
    }

    uint32_t size = sizeof(uint32_t) * 2;
    uint32_t current = size;

    uint32_t *properties_size = (uint32_t *)calloc(n_properties, sizeof(uint32_t));
    char **properties = (char **)calloc(n_properties, sizeof(char *));
    uint32_t *child_size = (uint32_t *)calloc(n_children, sizeof(uint32_t));
    char **child = (char **)calloc(n_children, sizeof(char *));

    int iprops = 0, inodes = 0;
    cJSON_ArrayForEach(sub, node) {
        if (cJSON_IsObject(sub)) {
            properties[iprops] = getPropertyChar(sub);
            cJSON *len_item = cJSON_GetObjectItemCaseSensitive(sub, "length");
            int len = len_item ? len_item->valueint : 0;
            properties_size[iprops] = 32 + sizeof(uint32_t) + ((len + 0x3) & ~0x3);
            size += properties_size[iprops];
            iprops++;
        } else if (cJSON_IsArray(sub)) {
            uint32_t c_size = 0;
            child[inodes] = getNodeChar(sub, &c_size);
            child_size[inodes] = c_size;
            size += child_size[inodes];
            inodes++;
        }
    }

    char *result = (char *)calloc(1, size);
    if (result) {
        memcpy(result, &n_properties, sizeof(n_properties));
        memcpy(&result[sizeof(uint32_t)], &n_children, sizeof(n_children));

        for (uint32_t i = 0; i < n_properties; i++) {
            if (properties[i]) {
                memcpy(&result[current], properties[i], properties_size[i]);
                free(properties[i]);
            }
            current += properties_size[i];
        }
        for (uint32_t i = 0; i < n_children; i++) {
            if (child[i]) {
                memcpy(&result[current], child[i], child_size[i]);
                free(child[i]);
            }
            current += child_size[i];
        }
    }

    free(properties_size);
    free(properties);
    free(child_size);
    free(child);

    if (full_size) {
        *full_size = size;
    }
    return result;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <devicetree.json> <output>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_data = (char *)malloc(fsize + 1);
    fread(json_data, 1, fsize, f);
    fclose(f);
    json_data[fsize] = '\0';

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root) {
        fprintf(stderr, "Error parsing JSON before: [%s]\n", cJSON_GetErrorPtr());
        return 1;
    }

    cJSON *dt = cJSON_GetObjectItemCaseSensitive(root, "device-tree");
    if (!dt || !cJSON_IsArray(dt)) {
        fprintf(stderr, "Invalid format: 'device-tree' key missing or not an array.\n");
        cJSON_Delete(root);
        return 1;
    }

    FILE *file = fopen(argv[2], "wb");
    if (!file) {
        fprintf(stderr, "Error opening output file: %s\n", strerror(errno));
        cJSON_Delete(root);
        return 1;
    }

    uint32_t node_size = 0;
    char *node_str = getNodeChar(dt, &node_size);
    if (node_str) {
        fwrite(node_str, node_size, 1, file);
        free(node_str);
    }

    fclose(file);
    cJSON_Delete(root);
    return 0;
}
