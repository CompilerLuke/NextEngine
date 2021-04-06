#include "gap.h"
#include <assert.h>
#include <string.h>

void remove_char(GapBuffer& gap) {
    if (gap.gap_begin > 0) gap.gap_begin--;
}

u64 upper_power_of_two(u64 v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

void ensure_gap(GapBuffer& gap, uint n) {
    uint gap_length = gap.gap_end - gap.gap_begin;
    
    if (gap_length < n) {
        gap.gap_target = max(gap.gap_target == 0 ? 8 : gap.gap_target * 2, upper_power_of_two(n));
        gap.gap_end = gap.gap_begin + gap.gap_target;
        gap.length += gap.gap_target;
        
        if (gap.length < gap.capacity) {
            for (uint i = gap.gap_begin; i < gap.length; i++) {
                gap.buffer[i+gap.gap_target] = gap.buffer[i]; //shift by gap_length
            }
        } else {
            gap.capacity = max(gap.capacity*2, upper_power_of_two(gap.length));
            
            char* buffer = (char*)malloc(gap.capacity);
            memcpy(buffer, gap.buffer, gap.gap_begin);
            memcpy(buffer + gap.gap_end, gap.buffer + gap.gap_begin, gap.length-gap.gap_end);
            
            if (gap.buffer) free(gap.buffer);
            gap.buffer = buffer;
        }
    }
}

void insert_char(GapBuffer& gap, char c) {
    ensure_gap(gap, 1);
    gap.buffer[gap.gap_begin++]= c;
}

int clamp(int n, int low, int high) {
    if (n < low) n = low;
    if (n > high) n = high;
    return n;
}

void move_cursor(GapBuffer& gap, int n) {
    n = clamp(n, -gap.gap_begin, gap.length-gap.gap_begin-1);
    for (int i = 0; i < n; i++) {
        gap.buffer[gap.gap_begin+i] = gap.buffer[gap.gap_end+i];
    }
    for (int i = 0; i < -n; i++) {
        gap.buffer[gap.gap_end-i-1] = gap.buffer[gap.gap_begin-i-1];
    }
    gap.gap_begin += n;
    gap.gap_end += n;
}
