#include "merge.h"

// int merge(vcf_record_t **variants, int num_variants, list_t* output_list) {
int merge(array_list_t **records_by_position, int num_positions, vcf_file_t **files, int num_files, merge_options_data_t *options, list_t *output_list) {
    // TODO get samples list, sorted by file (as provided as input)
//     array_list_t *samples = get_global_samples(files, num_files);
//     array_list_print(samples);
    
    // TODO
    vcf_record_t *merged;
    for (int i = 0; i < num_positions; i++) {
        if (records_by_position[i]->size == 1) {
            // TODO merge position present just in one file
            merged = merge_unique_position(records_by_position[i]->items[0], files, num_files, options);
        } else if (records_by_position[i]->size > 1) {
            // TODO merge position present in more than one file
            int info;
            merged = merge_shared_position((vcf_record_file_link**) records_by_position[i], num_positions, files, num_files, options, &info);
        }
        
        list_item_t *item = list_item_new(1, 1, merged);
        list_insert_item(item, output_list);
    }
    
    printf("Merging %d variants...\n", num_positions);
    
//     array_list_free(samples, free);
    
    return 0;
}

vcf_record_t *merge_unique_position(vcf_record_file_link *position, vcf_file_t **files, int num_files, merge_options_data_t *options) {
    vcf_record_t *input = position->record;
    vcf_record_t *result = vcf_record_new();
    set_vcf_record_chromosome(input->chromosome, input->chromosome_len, result);
    set_vcf_record_position(input->position, result);
    set_vcf_record_id(input->id, input->id_len, result);
    set_vcf_record_reference(input->reference, input->reference_len, result);
    set_vcf_record_alternate(input->alternate, input->alternate_len, result);
    set_vcf_record_quality(input->quality, result);
    set_vcf_record_filter(input->filter, input->filter_len, result);
    set_vcf_record_format(input->format, input->format_len, result);
    
    int num_format_fields = 1;
//     for (int i = 0; i < strlen(result->format); i++) {
    for (int i = 0; i < result->format_len; i++) {
        if (result->format[i] == ':') {
            num_format_fields++;
        }
    }
    char *format_bak = strndup(result->format, result->format_len);
    int gt_pos = get_field_position_in_format("GT", format_bak);
    
    // Create the text for empty samples
    char *empty_sample = get_empty_sample(num_format_fields, gt_pos, options->missing_mode);
    
    // Fill list of samples
    for (int i = 0; i < num_files; i++) {
        int file_num_samples = get_num_vcf_samples(files[i]);
        if(!strcmp(files[i]->filename, position->file->filename)) {
            // Samples of the file where the position has been read are directly copied
            array_list_insert_all(input->samples->items, file_num_samples, result->samples);
            LOG_DEBUG_F("%d samples of file %s inserted\n", file_num_samples, files[i]->filename);
        } else {
            // Samples in the rest of files are empty and must be filled according to the specified format
            for (int j = 0; j < file_num_samples; j++) {
                array_list_insert(strdup(empty_sample), result->samples);
            }
            LOG_DEBUG_F("file %s\t%d empty samples inserted\n", files[i]->filename, file_num_samples);
        }
    }
    
    // INFO field must be calculated based on statistics about the new list of samples
    // TODO add info-fields as argument (AF, NS and so on)
    cp_hashtable *alleles_table = cp_hashtable_create(6, cp_hash_istring, (cp_compare_fn) strcasecmp);
    char *alternate = merge_alternate_field(&position, 1, alleles_table);
    free(alternate);
    
    result->info = merge_info_field(options->info_fields, options->num_info_fields, &position, 1, result, alleles_table, empty_sample);
    
    return result;
}

vcf_record_t *merge_shared_position(vcf_record_file_link **position_in_files, int position_occurrences, 
                                    vcf_file_t **files, int num_files, merge_options_data_t *options, int *info) {
    vcf_file_t *file;
    vcf_record_t *input;
    vcf_record_t *result = vcf_record_new();
    
    // Check consistency among chromosome, position and reference
    for (int i = 0; i < position_occurrences-1; i++) {
        assert(position_in_files[i]);
        assert(position_in_files[i+1]);
        if (strncmp(position_in_files[i]->record->chromosome, position_in_files[i+1]->record->chromosome, position_in_files[i]->record->chromosome_len)) {
            LOG_ERROR("Positions can't be merged: Discordant chromosome\n");
            *info = DISCORDANT_CHROMOSOME;
            return NULL;
        } else if (position_in_files[i]->record->position != position_in_files[i+1]->record->position) {
            LOG_ERROR("Positions can't be merged: Discordant position\n");
            *info = DISCORDANT_POSITION;
            return NULL;
        } else if (strncmp(position_in_files[i]->record->reference, position_in_files[i+1]->record->reference, position_in_files[i]->record->reference_len)) {
            LOG_ERROR("Positions can't be merged: Discordant reference allele\n");
            *info = DISCORDANT_REFERENCE;
            return NULL;
        }
    }
    
    // Once checked, copy constant fields
    set_vcf_record_chromosome(position_in_files[0]->record->chromosome, position_in_files[0]->record->chromosome_len, result);
    set_vcf_record_position(position_in_files[0]->record->position, result);
    set_vcf_record_reference(position_in_files[0]->record->reference, position_in_files[0]->record->reference_len, result);
    
    // Get first non-dot ID
    // TODO what can we do when having several ID?
    result->id = merge_id_field(position_in_files, position_occurrences);
    result->id_len = strlen(result->id);
    
    // Calculate weighted mean of the quality
    result->quality = merge_quality_field(position_in_files, position_occurrences);
    
    // Concatenate alternates and set their order (used later to assign samples' alleles number)
    cp_hashtable *alleles_table = cp_hashtable_create(8, cp_hash_istring, (cp_compare_fn) strcasecmp);
    result->alternate = merge_alternate_field(position_in_files, position_occurrences, alleles_table);
    result->alternate_len = strlen(result->alternate);
    
    // Get the union of all FORMAT fields with their corresponding position
    // TODO include INFO fields in FORMAT
    array_list_t *format_fields = array_list_new(16, 1.2, COLLECTION_MODE_ASYNCHRONIZED);
    format_fields->compare_fn = strcasecmp;
    result->format = merge_format_field(position_in_files, position_occurrences, format_fields);
    result->format_len = strlen(result->format);
    
    // Get indexes for the format in each file
    // TODO Illustrate with an example
    int *format_indices = get_format_indices_per_file(position_in_files, position_occurrences, files, num_files, format_fields);
    
    // Create the text for empty samples
    char *format_bak = strndup(result->format, result->format_len);
    int gt_pos = get_field_position_in_format("GT", format_bak);
    char *empty_sample = get_empty_sample(format_fields->size, gt_pos, options->missing_mode);
//     size_t empty_sample_len = strlen(empty_sample);
    printf("empty sample = %s\n", empty_sample);
    
    // Generate samples using the reordered FORMAT fields and the new alleles' numerical values
    array_list_free(result->samples, NULL);
    result->samples = merge_samples(position_in_files, position_occurrences, files, num_files, 
                                    gt_pos, alleles_table, format_fields, format_indices, empty_sample);
    
    // TODO merge INFO field
    result->info = merge_info_field(options->info_fields, options->num_info_fields, position_in_files, position_occurrences, 
                                    result, alleles_table, empty_sample);
    
    array_list_free(format_fields, free);
    cp_hashtable_destroy(alleles_table);
    
    return result;
}


/* ******************************
 *    Field merging functions   *
 * ******************************/

char* merge_id_field(vcf_record_file_link** position_in_files, int position_occurrences) {
    vcf_record_t *input;
    char *result = NULL;
    
    for (int i = 0; i < position_occurrences; i++) {
        input = position_in_files[i]->record;
        if (strncmp(".", input->id, input->id_len)) {
            result = strndup(input->id, input->id_len);
            break;
        }
    }
    
    if (result == NULL) {
        result = strdup(".");
    }
    
    return result;
}


char* merge_alternate_field(vcf_record_file_link** position_in_files, int position_occurrences, cp_hashtable* alleles_table) {
    vcf_file_t *file;
    vcf_record_t *input;
    size_t max_len = 0, concat_len = 0;
    char *alternates = NULL, *cur_alternate = NULL, *aux = NULL;
    
    int cur_index = 0;
    int *allele_index = (int*) calloc (1, sizeof(int)); *allele_index = cur_index;
    cp_hashtable_put(alleles_table,
                     strndup(position_in_files[0]->record->reference, position_in_files[0]->record->reference_len), 
                     allele_index);
    
    for (int i = 0; i < position_occurrences; i++) {
        file = position_in_files[i]->file;
        input = position_in_files[i]->record;
        
        // TODO it doesn't work for multiallelic variants!
        cur_alternate = strndup(input->alternate, input->alternate_len);
        
        if (!alternates) {
            alternates = strndup(cur_alternate, input->alternate_len);
            max_len = input->alternate_len;
            
            allele_index = (int*) calloc (1, sizeof(int));
            *allele_index = ++cur_index;
            cp_hashtable_put(alleles_table, cur_alternate, allele_index);
        } else {
            if (!cp_hashtable_contains(alleles_table, cur_alternate)) {
                concat_len = input->alternate_len;
                aux = realloc(alternates, max_len + concat_len + 2);
                if (aux) {
                    // Concatenate alternate value to the existing list
                    strncat(aux, ",", 1);
//                     printf("1) cur_alternate = %.*s\naux = %s\n---------\n", concat_len, cur_alternate, aux);
                    strncat(aux, cur_alternate, concat_len);
                    alternates = aux;
                    max_len += concat_len + 1;
//                     printf("2) alternates = %s\n---------\n", alternates);
                    
                    // In case the allele is not in the hashtable, insert it with a new index
                    allele_index = (int*) calloc (1, sizeof(int));
                    *allele_index = ++cur_index;
                    cp_hashtable_put(alleles_table, cur_alternate, allele_index);
                } else {
                    LOG_FATAL_F("Can't allocate memory for alternate alleles in position %s:%ld\n", 
                                input->chromosome, input->position);
                }
            }
        }
    }

    return alternates;
}


float merge_quality_field(vcf_record_file_link** position_in_files, int position_occurrences) {
    vcf_file_t *file;
    vcf_record_t *input;
    float accum_quality = 0.0f;
    int total_samples = 0;
    int file_num_samples;
    float result;
    
    for (int i = 0; i < position_occurrences; i++) {
        file = position_in_files[i]->file;
        input = position_in_files[i]->record;
        file_num_samples = get_num_vcf_samples(file);
        
        if (input->quality > 0) {
            accum_quality += input->quality * file_num_samples;
        }
        total_samples += file_num_samples;
    }
    
    if (total_samples > 0) {
        result = accum_quality / total_samples;
    } else {
        result = -1;
    }
    
    return result;
}


char* merge_filter_field(vcf_record_file_link** position_in_files, int position_occurrences) {
    char *result;   
    vcf_record_t *input;
    int filter_text_len = 0;
    array_list_t *failed_filters = array_list_new(8, 1.2, COLLECTION_MODE_ASYNCHRONIZED);
    failed_filters->compare_fn = strcasecmp;
    int *field_index = (int*) calloc (1, sizeof(int)); *field_index = 0;
    
    // Flags
    int pass_found = 0;
    int miss_found = 0;
    
    // Register all failed filters
    for (int i = 0; i < position_occurrences; i++) {
        input = position_in_files[i]->record;
        
        if (!strncmp(input->filter, "PASS", input->filter_len)) {
            pass_found = 1;
        } else if (!strncmp(input->filter, ".", input->filter_len)) {
            miss_found = 1;
        } else {
            char *filter = strndup(input->filter, input->filter_len);
            if (!array_list_contains(filter, failed_filters)) {
                array_list_insert(filter, failed_filters);
                filter_text_len += strlen(filter) + 1; // concat field + ","
            }
        }
    }
    
    // Should there be any failed filters, write them as result
    if (failed_filters->size > 0) {
        result = calloc (filter_text_len + 1, sizeof(char));
        strcat(result, (char*) array_list_get(0, failed_filters));
        for (int i = 1; i < failed_filters->size; i++) {
            strncat(result, ";", 1);
            strcat(result, (char*) array_list_get(i, failed_filters));
        }
    } else {
        if (pass_found) {
            result = strndup("PASS", 4);
        } else {
            result = strndup(".", 1);
        }
    }
    
    return result;
}


char *merge_info_field(char **info_fields, int num_fields, vcf_record_file_link **position_in_files, int position_occurrences, 
                       vcf_record_t *output_record, cp_hashtable *alleles, char *empty_sample) {
    size_t len = 0;
    size_t max_len = 128;
    char *result = calloc (max_len, sizeof(char));
    char *aux;
    
    list_t *stats_list = malloc (sizeof(list_t));
    list_init("stats", 1, INT_MAX, stats_list);
    file_stats_t *file_stats = file_stats_new();
    variant_stats_t *variant_stats;
    int dp = 0, mq0 = 0;
    int dp_checked = 0, mq_checked = 0, stats_checked = 0;
    double mq = 0;
    
    for (int i = 0; i < num_fields; i++) {
//         printf("\n----------\nfield = %s\nresult = %s\n--------\n", info_fields[i], result);
        if (len >= max_len - 32) {
            aux = realloc(result, max_len + 128);
            if (aux) {
                result = aux;
                max_len += 128;
            } else {
                LOG_FATAL("Can't allocate memory for file merging");
            }
        }
        
        // Conditional precalculations
        // TODO will it be faster to calculate these once and for all or keep asking strncmp?
        
        if (!stats_checked && 
            (!strncmp(info_fields[i], "AC", 2) ||   // allele count in genotypes, for each ALT allele
             !strncmp(info_fields[i], "AF", 2))) {  // allele frequency for each ALT allele
            get_variants_stats(&output_record, 1, stats_list, file_stats);
            variant_stats = list_remove_item(stats_list)->data_p;
        }
        
        if (!dp_checked && 
            (!strncmp(info_fields[i], "DP", 2) ||    // combined depth across samples
             !strncmp(info_fields[i], "QD", 2))) {   // quality by depth (GATK)
            int dp_pos = -1;
            for (int j = 0; j < output_record->samples->size; j++) {
                dp_pos = get_field_position_in_format("DP", strndup(output_record->format, output_record->format_len));
                if (dp_pos >= 0) {
                    dp += atoi(get_field_value_in_sample(strdup((char*) array_list_get(j, output_record->samples)), dp_pos));
                }
            }
            dp_checked = 1;
        }
        
        if (!mq_checked &&
            (!strncmp(info_fields[i], "MQ0", 3) ||    // Number of MAPQ == 0 reads covering this record
             !strncmp(info_fields[i], "MQ", 2))) {    // RMS mapping quality
            int mq_pos;
            int cur_gq;
            for (int j = 0; j < output_record->samples->size; j++) {
                mq_pos = get_field_position_in_format("GQ", strndup(output_record->format, output_record->format_len));
                if (mq_pos < 0) {
                    continue;
                }
                
                cur_gq = atoi(get_field_value_in_sample(strdup((char*) array_list_get(j, output_record->samples)), mq_pos));
//                 printf("sample = %s\tmq_pos = %d\tvalue = %d\n", array_list_get(j, record->samples), mq_pos,
//                        atoi(get_field_value_in_sample(strdup((char*) array_list_get(j, record->samples)), mq_pos)));
                if (cur_gq == 0) {
                    mq0++;
                } else {
                    mq += cur_gq * cur_gq;
                }
            }
            mq = sqrt(mq / output_record->samples->size);
            mq_checked = 1;
        }
        
        // Composition of the INFO field
        
        if (!strncmp(info_fields[i], "AC", 2)) {   // allele count in genotypes, for each ALT allele
            strncat(result, "AC=", 3);
            len += 3;
            for (int j = 1; j < variant_stats->num_alleles; j++) {
                if (j < variant_stats->num_alleles - 1) {
                    sprintf(result+len, "%d,", variant_stats->alleles_count[j]);
                } else {
                    sprintf(result+len, "%d;", variant_stats->alleles_count[j]);
                }
                len = strlen(result);
            }
            
        } else if (!strncmp(info_fields[i], "AF", 2)) {   // allele frequency for each ALT allele
            // For each ALT, AF = alt_freq / (1 - ref_freq)
            strncat(result, "AF=", 3);
            len += 3;
            for (int j = 1; j < variant_stats->num_alleles; j++) {
                if (j < variant_stats->num_alleles - 1) {
                    sprintf(result+len, "%.3f,", variant_stats->alleles_freq[j] / (1 - variant_stats->alleles_freq[0]));
                } else {
                    sprintf(result+len, "%.3f;", variant_stats->alleles_freq[j] / (1 - variant_stats->alleles_freq[0]));
                }
                len = strlen(result);
            }
            
        } else if (!strncmp(info_fields[i], "AN", 2)) {    // total number of alleles in called genotypes
            sprintf(result+len, "AN=%d;", cp_hashtable_count(alleles));
            len = strlen(result);
            
        } else if (!strncmp(info_fields[i], "DB", 2)) {    // dbSNP membership
            for (int j = 0; j < position_occurrences; j++) {
                if (strstr(position_in_files[j]->record->info, "DB")) {
                    strncat(result, "DB;", 3);
                    len += 3;
                    break;
                }
            }
            
        } else if (!strncmp(info_fields[i], "DP", 2)) {    // combined depth across samples
            sprintf(result+len, "DP=%d;", dp);
            len = strlen(result);
            
        } else if (!strncmp(info_fields[i], "H2", 2)) {    // membership in hapmap2
            for (int j = 0; j < position_occurrences; j++) {
                if (strstr(position_in_files[j]->record->info, "H2")) {
                    strncat(result, "H2;", 3);
                    len += 3;
                    break;
                }
            }
            
        } else if (!strncmp(info_fields[i], "H3", 2)) {    // membership in hapmap3
            for (int j = 0; j < position_occurrences; j++) {
                if (strstr(position_in_files[j]->record->info, "H3")) {
                    strncat(result, "H3;", 3);
                    len += 3;
                    break;
                }
            }
            
        } else if (!strncmp(info_fields[i], "MQ0", 3)) {   // Number of MAPQ == 0 reads covering this record
            sprintf(result+len, "MQ0=%d;", mq0);
            len = strlen(result);
            
        } else if (!strncmp(info_fields[i], "MQ", 2)) {    // RMS mapping quality
            sprintf(result+len, "MQ=%.3f;", mq);
            len = strlen(result);
            
        } else if (!strncmp(info_fields[i], "NS", 2)) {    // Number of samples with data
            int ns = 0;
            for (int j = 0; j < output_record->samples->size; j++) {
                if (strcmp(array_list_get(j, output_record->samples), empty_sample)) {
                    ns++;
                }
            }
            sprintf(result+len, "NS=%d;", ns);
            len = strlen(result);
            
        } else if (!strncmp(info_fields[i], "QD", 2)) {    // quality by depth (GATK)
            if (output_record->quality < 0) {
                strncat(result, "QD=.;", 1);
                len += 5;
            } else if (output_record->quality > 0) {
                sprintf(result+len, "QD=%.3f;", output_record->quality / dp);
                len = strlen(result);
            } else {
                strncat(result, "QD=0.000;", 1);
                len += 5;
            }
            
        } else if (!strncmp(info_fields[i], "SOMATIC", 7)) {   // the record is a somatic mutation, for cancer genomics
            for (int j = 0; j < position_occurrences; j++) {
                if (strstr(position_in_files[j]->record->info, "SOMATIC")) {
                    strncat(result, "SOMATIC;", 8);
                    len += 8;
                    break;
                }
            }
            
        } else if (!strncmp(info_fields[i], "VALIDATED", 9)) { // validated by follow-up experiment
            for (int j = 0; j < position_occurrences; j++) {
                if (strstr(position_in_files[j]->record->info, "SOMATIC")) {
                    strncat(result, "VALIDATED;", 10);
                    len += 10;
                    break;
                }
            }
        }
    }
    
    result[len-1] = '\0';
    
    free(file_stats);
    
    return result;
}


char* merge_format_field(vcf_record_file_link** position_in_files, int position_occurrences, array_list_t* format_fields) {
    char *result;   
    vcf_record_t *input;
    int format_text_len = 0;
    
    // Split FORMAT of the input record and register the non-previously inserted ones
    for (int i = 0; i < position_occurrences; i++) {
        input = position_in_files[i]->record;
        int num_fields;
        char **fields = split(strndup(input->format, input->format_len), ":", &num_fields);
        for (int j = 0; j < num_fields; j++) {
            if (!array_list_contains(fields[j], format_fields)) {
                array_list_insert(fields[j], format_fields);
                format_text_len += strlen(fields[j]) + 1; // concat field + ":"
            }
        }
    }
    
    result = calloc (format_text_len + 1, sizeof(char));
    strcat(result, (char*) array_list_get(0, format_fields));
    for (int i = 1; i < format_fields->size; i++) {
        strncat(result, ":", 1);
        strcat(result, (char*) array_list_get(i, format_fields));
    }
    
    return result;
}

array_list_t* merge_samples(vcf_record_file_link** position_in_files, int position_occurrences, vcf_file_t **files, int num_files, 
                            int gt_pos, cp_hashtable* alleles_table, array_list_t* format_fields, int *format_indices, char *empty_sample) {
    int empty_sample_len = strlen(empty_sample);
    array_list_t *result = array_list_new(num_files * 2, 1.5, COLLECTION_MODE_ASYNCHRONIZED);
    int aux_idx;
    
    for (int i = 0; i < num_files; i++) {
        vcf_record_file_link *link = NULL;
        for (int j = 0; j < position_occurrences; j++) {
            if (!strcmp(position_in_files[j]->file->filename, files[i]->filename)) {
                link = position_in_files[j];
                break;
            }
        }
        
        if (link) {
            vcf_record_t *record = link->record;
            int num_sample_fields;
            char sample[1024];
            int len;
            for (int j = 0; j < get_num_vcf_samples(files[i]); j++) {
                memset(sample, 0, 256 * sizeof(char));
                len = 0;
                
                char **split_sample = split(strdup(array_list_get(j, record->samples)), ":", &num_sample_fields);
                for (int k = 0; k < format_fields->size; k++) {
                    int idx = format_indices[i*format_fields->size + k];
//                     printf("k = %d\tidx = %d\n", k, format_indices[i*format_fields->size + k]);
                    if (idx < 0) {  // Missing
                        if (k == gt_pos) {
                            strncat(sample, "./.", 3);
                            len += 3;
                        } else {
                            strncat(sample, ".", 1);
                            len++;
                        }
                    } else {    // Not-missing
                        if (k == gt_pos) {
                            // Manage genotypes (in multiallelic variants, allele indices must be recalculated)
                            int allele1, allele2;
                            int allele_ret = get_alleles(strdup(split_sample[idx]), 0, &allele1, &allele2);
                            if (allele_ret == 3) {
                                strncat(sample, "./.", 3);
                                len += 3;
                            } else {
                                if (allele1 < 0) {
                                    strncat(sample, ".", 1);
                                } else if (allele1 == 0) {
                                    strncat(sample, "0", 1);
                                } else {
//                                     printf("alternate = %.*s\nidx = %d\n", 
//                                            record->alternate_len, record->alternate, 
//                                            *((int*) cp_hashtable_get(alleles_table, record->alternate)));
                                    aux_idx = *((int*) cp_hashtable_get(alleles_table, record->alternate));
                                    sprintf(sample + len, "%d", aux_idx);
                                }
                                len++;
                                
                                strncat(sample, split_sample[idx] + 1, 1);
                                len++;
                                
                                if (allele2 < 0) {
                                    strncat(sample, ".", 1);
                                } else if (allele2 == 0) {
                                    strncat(sample, "0", 1);
                                } else {
                                    aux_idx = *((int*) cp_hashtable_get(alleles_table, record->alternate));
                                    sprintf(sample + len, "%d", aux_idx);
                                }
                                len++;
                            }
                        } else {
//                             printf("sample = %s\n", array_list_get(j, record->samples));
                            strcat(sample, split_sample[idx]);
                            len += strlen(split_sample[idx]);
                        }
                    }
                    
                    
                    if (k < format_fields->size - 1) {
                        strncat(sample, ":", 1);
                        len += 1;
                    }
                }
                
                sample[len] = '\0';
//                 printf("sample = %s\n", sample);
                array_list_insert(strndup(sample, len), result);
            }
            
        } else {
            // If the file has no samples in that position, fill with empty samples
            for (int j = 0; j < get_num_vcf_samples(files[i]); j++) {
                array_list_insert(strndup(empty_sample, empty_sample_len), result);
            }
        }
    }
    
    return result;
}


/* ******************************
 *      Auxiliary functions     *
 * ******************************/

int *get_format_indices_per_file(vcf_record_file_link **position_in_files, int position_occurrences, 
                                    vcf_file_t **files, int num_files, array_list_t *format_fields) {
    int *indices = malloc (format_fields->size * num_files * sizeof(size_t));
    int in_file = 0;
    vcf_record_t *record;
    char *split_format;
    for (int i = 0; i < num_files; i++) {
        in_file = 0;
        
        for (int j = 0; j < position_occurrences; j++) {
            // If the position is in this file, then get the record format
            assert(files[i]);
            assert(position_in_files[j]);
            if (!strcmp(position_in_files[j]->file->filename, files[i]->filename)) {
                in_file = 1;
                record = position_in_files[j]->record;
                int num_fields;
                char **fields = split(strndup(record->format, record->format_len), ":", &num_fields);
                
                for (int k = 0; k < format_fields->size; k++) {
                    indices[i*format_fields->size + k] = -1;
                    
                    for (int m = 0; m < num_fields; m++) {
                        if (!strcmp(array_list_get(k, format_fields), fields[m])) {
                            indices[i*format_fields->size + k] = m;
                            break;
                        }
                    }
                }
                
                break;
            }
        }
        
        // If the position is not in the file, set all positions as -1
        if (!in_file) {
            for (int k = 0; k < format_fields->size; k++) {
                indices[i*format_fields->size + k] = -1;
            }
        }
    }
    
    return indices;
}

array_list_t *get_global_samples(vcf_file_t **files, int num_files) {
    array_list_t *samples = array_list_new(get_num_vcf_samples(files[0]) * 2, 2, COLLECTION_MODE_ASYNCHRONIZED);
    for (int i = 0; i < num_files; i++) {
        array_list_insert_all(files[i]->samples_names->items, files[i]->samples_names->size, samples);
    }
    return samples;
}

char *get_empty_sample(int num_format_fields, int gt_pos, enum missing_mode mode) {
    int sample_len = num_format_fields * 2 + 2; // Each field + ':' = 2 chars, except for GT which is 1 char more
    char *sample = (char*) calloc (sample_len, sizeof(char));
    for (int j = 0; j < num_format_fields; j++) {
        if (j > 0) {
            strncat(sample, ":", 1);
        }
        if (j != gt_pos) {
            strncat(sample, ".", 1);
        } else {
            if (mode == MISSING) {
                strncat(sample, "./.", 3);
            } else if (mode == REFERENCE) {
                strncat(sample, "0/0", 3);
            }
        }
    }
    return sample;
}
