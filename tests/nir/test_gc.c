#include <stdio.h>
#include <assert.h>
#include <string.h>

/*
 * Host-side mark-and-sweep collector implementing the algorithm specified for
 * the Nuru runtime (see runtime/GC.md). Objects form a graph; roots are the
 * live references (evaluation stack + globals). Marking is a recursive walk
 * from roots; sweeping reclaims unmarked objects.
 */

#define MAX_OBJ 256
#define MAX_REFS 8

typedef struct Obj {
    int alive;     /* allocated this cycle */
    int marked;    /* reached from roots in the current collection */
    int nrefs;
    int refs[MAX_REFS];
} Obj;

typedef struct Arena {
    Obj objs[MAX_OBJ];
    int count;
} Arena;

static Arena G;

static int new_obj(int nrefs) {
    assert(G.count < MAX_OBJ);
    int i = G.count++;
    memset(&G.objs[i], 0, sizeof(Obj));
    G.objs[i].alive = 1;
    G.objs[i].nrefs = nrefs;
    return i;
}

static void set_ref(int o, int k, int target) {
    G.objs[o].refs[k] = target;
}

static void mark_from(int o) {
    if (o < 0 || !G.objs[o].alive || G.objs[o].marked) return;
    G.objs[o].marked = 1;
    for (int k = 0; k < G.objs[o].nrefs; k++) {
        mark_from(G.objs[o].refs[k]);
    }
}

static void gc_collect(int* roots, int nroots) {
    for (int i = 0; i < G.count; i++) G.objs[i].marked = 0;
    for (int i = 0; i < nroots; i++) mark_from(roots[i]);
    for (int i = 0; i < G.count; i++) {
        if (G.objs[i].alive && !G.objs[i].marked) {
            G.objs[i].alive = 0; /* reclaimed */
        }
    }
}

static int alive(int o) { return o >= 0 && G.objs[o].alive; }

static void reset_arena(void) { memset(&G, 0, sizeof(G)); }

/* --- tests --- */

static void test_reachable_preserved(void) {
    reset_arena();
    int root = new_obj(1);
    int mid = new_obj(1);
    int leaf = new_obj(0);
    set_ref(root, 0, mid);
    set_ref(mid, 0, leaf);
    int dead = new_obj(1);      /* unreachable */
    set_ref(dead, 0, root);
    int roots[] = { root };
    gc_collect(roots, 1);
    assert(alive(root) && alive(mid) && alive(leaf));
    assert(!alive(dead));
    printf("PASS  gc_reachable_preserved\n");
}

static void test_cycle_reclaimed(void) {
    reset_arena();
    int a = new_obj(1);
    int b = new_obj(1);
    set_ref(a, 0, b);
    set_ref(b, 0, a);          /* cycle, unreachable */
    gc_collect(NULL, 0);
    assert(!alive(a) && !alive(b));
    printf("PASS  gc_cycle_reclaimed\n");
}

static void test_reachable_cycle_preserved(void) {
    reset_arena();
    int root = new_obj(1);
    int a = new_obj(1);
    int b = new_obj(1);
    set_ref(root, 0, a);
    set_ref(a, 0, b);
    set_ref(b, 0, a);          /* cycle reachable from root */
    int roots[] = { root };
    gc_collect(roots, 1);
    assert(alive(root) && alive(a) && alive(b));
    printf("PASS  gc_reachable_cycle_preserved\n");
}

static void test_sweep_resets_mark(void) {
    reset_arena();
    int x = new_obj(1);
    int y = new_obj(0);
    set_ref(x, 0, y);
    int roots[] = { x };
    gc_collect(roots, 1);
    assert(alive(x) && alive(y));
    /* second collection: y no longer reachable */
    G.objs[x].nrefs = 0;
    gc_collect(roots, 1);
    assert(alive(x) && !alive(y));
    printf("PASS  gc_sweep_resets_mark\n");
}

int main(void) {
    test_reachable_preserved();
    test_cycle_reclaimed();
    test_reachable_cycle_preserved();
    test_sweep_resets_mark();
    printf("gc tests: 4 passed\n");
    return 0;
}
