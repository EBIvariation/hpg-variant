/*
 * Copyright (c) 2012-2014 Cristina Yenyxe Gonzalez Garcia (EMBL-EBI)
 * Copyright (c) 2012-2014 Ignacio Medina (EMBL-EBI)
 *
 * This file is part of hpg-variant.
 *
 * hpg-variant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * hpg-variant is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hpg-variant. If not, see <http://www.gnu.org/licenses/>.
 */

#include "aggregate.h"


int read_aggregate_configuration(const char *filename, aggregate_options_t *options, shared_options_t *shared_options) {
    if (filename == NULL || options == NULL || shared_options == NULL) {
        return -1;
    }
    
    config_t *config = (config_t*) calloc (1, sizeof(config_t));
    int ret_code = config_read_file(config, filename);
    if (ret_code == CONFIG_FALSE) {
        LOG_ERROR_F("config file error: %s\n", config_error_text(config));
        return ret_code;
    }
    
    // Read number of threads that will make request to the web service
    ret_code = config_lookup_int(config, "vcf-tools.aggregate.num-threads", shared_options->num_threads->ival);
    if (ret_code == CONFIG_FALSE) {
        LOG_WARN("Number of threads not found in config file, must be set via command-line\n");
    } else {
        LOG_DEBUG_F("num-threads = %ld\n", *(shared_options->num_threads->ival));
    }

    // Read maximum number of batches that can be stored at certain moment
    ret_code = config_lookup_int(config, "vcf-tools.aggregate.max-batches", shared_options->max_batches->ival);
    if (ret_code == CONFIG_FALSE) {
        LOG_WARN("Maximum number of batches not found in configuration file, must be set via command-line\n");
    } else {
        LOG_DEBUG_F("max-batches = %ld\n", *(shared_options->max_batches->ival));
    }
    
    // Read size of every batch read
    ret_code = config_lookup_int(config, "vcf-tools.aggregate.batch-lines", shared_options->batch_lines->ival);
    ret_code |= config_lookup_int(config, "vcf-tools.aggregate.batch-bytes", shared_options->batch_bytes->ival);
    if (ret_code == CONFIG_FALSE) {
        LOG_WARN("Neither batch lines nor bytes found in configuration file, must be set via command-line\n");
    } 
    /*else {
        LOG_DEBUG_F("batch-lines = %ld\n", *(shared_options->batch_size->ival));
    }*/
    
    config_destroy(config);
    free(config);

    return 0;
}

void **parse_aggregate_options(int argc, char *argv[], aggregate_options_t *aggregate_options, shared_options_t *shared_options) {
    struct arg_end *end = arg_end(NUM_AGGREGATE_OPTIONS);
    void **argtable = merge_aggregate_options(aggregate_options, shared_options, end);
    
    int num_errors = arg_parse(argc, argv, argtable);
    if (num_errors > 0) {
        arg_print_errors(stdout, end, "hpg-var-vcf");
    }
    
    return argtable;
}

void **merge_aggregate_options(aggregate_options_t *aggregate_options, shared_options_t *shared_options, struct arg_end *arg_end) {
    void **tool_options = malloc (NUM_AGGREGATE_OPTIONS * sizeof(void*));
    // Input/output files
    tool_options[0] = shared_options->vcf_filename;
//    tool_options[1] = shared_options->ped_filename;
    tool_options[1] = shared_options->output_filename;
    tool_options[2] = shared_options->output_directory;
    
    // Aggregate arguments
    tool_options[3] = aggregate_options->overwrite;
//    tool_options[5] = aggregate_options->variable;
//    tool_options[6] = aggregate_options->variable_groups;
//    tool_options[7] = aggregate_options->phenotype;
    
    // Configuration file
    tool_options[4] = shared_options->log_level;
    tool_options[5] = shared_options->config_file;
    
    // Advanced configuration
    tool_options[6] = shared_options->max_batches;
    tool_options[7] = shared_options->batch_lines;
    tool_options[8] = shared_options->batch_bytes;
    tool_options[9] = shared_options->num_threads;
    tool_options[10] = shared_options->mmap_vcf_files;
    tool_options[11] = shared_options->compression;
    
    tool_options[12] = arg_end;
    
    return tool_options;
}


int verify_aggregate_options(aggregate_options_t *aggregate_options, shared_options_t *shared_options) {
    // Check whether the input VCF file is defined
    if (shared_options->vcf_filename->count == 0) {
        LOG_ERROR("Please specify the input VCF file.\n");
        return VCF_FILE_NOT_SPECIFIED;
    }
    
    // Check whether batch lines or bytes are defined
    if (*(shared_options->batch_lines->ival) == 0 && *(shared_options->batch_bytes->ival) == 0) {
        LOG_ERROR("Please specify the size of the reading batches (in lines or bytes).\n");
        return BATCH_SIZE_NOT_SPECIFIED;
    }
    
    // Check if both batch lines or bytes are defined
    if (*(shared_options->batch_lines->ival) > 0 && *(shared_options->batch_bytes->ival) > 0) {
        LOG_WARN("The size of reading batches has been specified both in lines and bytes. The size in bytes will be used.\n");
        return 0;
    }
    
    // If no PED file is specified, not all statistics can be calculated
    if (shared_options->ped_filename->count == 0) {
        LOG_WARN("Input PED file not specified: not all statistics can be calculated.\n");
    }
    
    return 0;
}

