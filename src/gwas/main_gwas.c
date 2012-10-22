/*
 * Copyright (c) 2012 Cristina Yenyxe Gonzalez Garcia (ICM-CIPF)
 * Copyright (c) 2012 Ignacio Medina (ICM-CIPF)
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

#include "main_gwas.h"

int main(int argc, char *argv[]) {
    if (argc == 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        printf("Usage: %s < assoc | tdt > < tool-options >\nFor more information about a certain tool, type %s tool-name --help\n", argv[0], argv[0]);
        return 0;
    }
    
    init_log_custom(2, 1, "hpg-var-gwas.log", "w");

    const char *config = find_configuration_file(argc, argv);
    
    // Get sub-tool
    char *tool = argv[1];
    int exit_code = 0;
    
    // Parse tool args and run tool
    if (strcmp(tool, "assoc") == 0) {
        exit_code = association(argc - 1, argv + 1, config);
        
    } else if (strcmp(tool, "tdt") == 0) {
        exit_code = tdt(argc - 1, argv + 1, config);
 
    } else {
        fprintf(stderr, "The requested genome-wide analysis tool does not exist! (%s)\n", tool);
        exit_code = NOT_IMPLEMENTED_TOOL;
    }
    
    if (exit_code > 0) {
        fprintf(stderr, "Tool %s terminated with failure (exit code = %d)\n", tool, exit_code);
    }
    
    stop_log();
    
    return exit_code;
}

