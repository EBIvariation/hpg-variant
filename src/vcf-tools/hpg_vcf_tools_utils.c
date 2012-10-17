#include "hpg_vcf_tools_utils.h"

char *find_configuration_file(int argc, char *argv[]) {
    FILE *config_file = NULL;
    char *config_filepath = NULL;
    for (int i = 0; i < argc-1; i++) {
        if (!strcmp("--config", argv[i])) {
            config_filepath = argv[i+1];
        }
    }
    if (!config_filepath) {
        config_filepath = "hpg-vcf-tools.cfg";
    }
    
    config_file = fopen(config_filepath, "r");
    if (!config_file) {
        LOG_FATAL("Configuration file can't be loaded!");
    } else {
        fclose(config_file);
    }
    
    LOG_INFO_F("Configuration file = %s\n", config_filepath);
    return config_filepath;
}


// list_item_t** create_chunks(list_t* records, int max_chunk_size, int *num_chunks) {
//     *num_chunks = (int) ceil((float) records->length / max_chunk_size);
//     LOG_DEBUG_F("%d chunks of %d elements top\n", *num_chunks, max_chunk_size);
//     
//     list_item_t **chunk_starts = (list_item_t**) malloc ((*num_chunks) * sizeof(list_item_t*));
//     list_item_t *current = records->first_p;
//     for (int j = 0; j < *num_chunks; j++) {
//         chunk_starts[j] = current;
//         for (int k = 0; k < max_chunk_size && current->next_p != NULL; k++) {
//             current = current->next_p;
//         }
//     }
// 
//     return chunk_starts;
// }

int *create_chunks(int length, int max_chunk_size, int *num_chunks, int **chunk_sizes) {
    *num_chunks = (int) ceil((float) length / max_chunk_size);
    int *chunk_starts = (int*) calloc (*(num_chunks), sizeof(int));
    *chunk_sizes = (int*) calloc (*(num_chunks), sizeof(int));
    LOG_DEBUG_F("%d chunks of %d elements top\n", *num_chunks, max_chunk_size);
    
    for (int j = 0; j < *num_chunks; j++) {
        chunk_starts[j] = j * max_chunk_size;
        (*chunk_sizes)[j] = max_chunk_size;
    }
    int mod = length % max_chunk_size;
    if (mod > 0) {
        (*chunk_sizes)[*num_chunks-1] = mod;
    }

    return chunk_starts;
}
