#pragma once

#include "core/core.h"

const uint GAP_LENGTH = 100;

struct GapBuffer {
    char* buffer;
    uint capacity;
    uint length;
    
    uint gap_begin;
    uint gap_end;
    uint gap_target;
};

void remove_char(GapBuffer& gap);
void insert_char(GapBuffer& gap, char c);
void move_cursor(GapBuffer& gap, int n);
