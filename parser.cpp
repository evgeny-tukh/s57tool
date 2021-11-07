#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include "s57defs.h"

size_t loadFile (const char *path, char *& content) {
    size_t size = 0;
    FILE *file = fopen (path, "rb+");

    if (file) {
        fseek (file, 0, SEEK_END);
        size = ftell (file);
        fseek (file, 0, SEEK_SET);
        content = (char *) malloc (size);
        
        if (!content) {
            size = 0;
        } else if (fread (content, 1, size, file) == 0) {
            free (content);
            size = 0;
        }

        fclose (file);
    }

    return size;
}

bool parseCatalog (const char *catPath) {
    char *catalogContent = 0;
    size_t size = loadFile (catPath, catalogContent);
    bool result = false;

    if (size) {
        result = true;
        free (catalogContent);
    }

    return result;
}

size_t parseRecord (const char *start) {
    
}