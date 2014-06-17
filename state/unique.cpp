// (c) 2007,2010-2011,2013 Coverity, Inc. All rights reserved worldwide.
#include "unique.hpp"
#include "analysis/work-unit/wu-analysis.hpp" // g_current_function_trav
vector<void (*)(void)>
clear_unique_objs_instances_vec;

static heap_allocated_arena_t *the_unique_arena;

arena_t *get_unique_obj_arena() {
    if(!the_unique_arena) {
        // Cap memory, based on g_current_function_trav.
        // In some benchmarks (e.g. test case in bug 52591), memory on
        // this arena was about equal to memory on the state arena, or
        // about half total memory usage.
        arena_parameters_t params("unique");
        g_current_function_trav->setMemCap(params);
        the_unique_arena =
            new heap_allocated_arena_t(params);
    }
    return the_unique_arena;
}

void clear_unique_objs() {
    foreach(i, clear_unique_objs_instances_vec) {
        (*i)();
    }
    clear_unique_objs_instances_vec.clear();
    if(the_unique_arena) {
        delete the_unique_arena;
        the_unique_arena = NULL;
    }
}

