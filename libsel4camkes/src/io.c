/*
 * Copyright 2015, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* IO port/device functionality. This is meant for interaction with
 * libplatsupport infrastructure.
 */

#include <assert.h>
#include <camkes/dma.h>
#include <camkes/io.h>
#include <gdsl/gdsl.h>
#include <platsupport/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <utils/util.h>

/* The following functions are generated in the component-wide template. */
extern void *camkes_io_map(void *cookie, uintptr_t paddr, size_t size,
    int cached, ps_mem_flags_t flags);
extern int camkes_io_port_in(void *cookie, uint32_t port, int io_size,
    uint32_t *result);
extern int camkes_io_port_out(void *cookie, uint32_t port, int io_size,
    uint32_t val);

typedef struct {
    ps_io_map_fn_t map;
    gdsl_list_t mapped;
} cookie_t;

/* Debug wrapper for IO map. This function calls the underlying map function
 * and tracks results for the purpose of catching illegal unmapping operations.
 * Note that this function is unused when NDEBUG is defined.
 */
static UNUSED void * io_map(void *cookie, uintptr_t paddr, size_t size,
        int cached, ps_mem_flags_t flags) {

    /* Call the real IO map function. */
    cookie_t *c = cookie;
    void *p = c->map(NULL, paddr, size, cached, flags);

    if (p != NULL) {
        /* The IO map function gave us a successful result; track this pointer
         * to lookup during unmapping.
         */
        if (gdsl_list_insert_head(c->mapped, p) == NULL) {
            LOG_ERROR("failed to track mapped IO pointer %p\n", p);
        }
    }

    return p;
}

static long int UNUSED pointer_compare(void *a, void *b) {
    return (long int)((uintptr_t)a - (uintptr_t)b);
}

/* We never unmap anything. */
static void io_unmap(void *cookie UNUSED, void *vaddr UNUSED, size_t size UNUSED) {
#ifndef NDEBUG
    cookie_t *c = cookie;
    /* Make sure we previously mapped the pointer the caller gave us. */
    if (gdsl_list_remove(c->mapped, pointer_compare, vaddr) == NULL) {
        LOG_ERROR("unmapping an IO pointer that was not previously mapped: %p\n",
            vaddr);
    }
#endif
}

int camkes_io_mapper(ps_io_mapper_t *mapper) {
    assert(mapper != NULL);
#ifdef NDEBUG
    mapper->cookie = NULL;
    mapper->io_map_fn = camkes_io_map;
#else
    cookie_t *c = malloc(sizeof(*c));
    if (c == NULL) {
        return -1;
    }
    c->map = camkes_io_map;
    c->mapped = gdsl_list_alloc("IO allocation debug tracking", NULL, NULL);
    if (c->mapped == NULL) {
        free(c);
        return -1;
    }
    mapper->cookie = c;
    mapper->io_map_fn = io_map;
#endif
    mapper->io_unmap_fn = io_unmap;
    return 0;
}

int camkes_io_port_ops(ps_io_port_ops_t *ops) {
    assert(ops != NULL);
    ops->io_port_in_fn = camkes_io_port_in;
    ops->io_port_out_fn = camkes_io_port_out;
    return 0;
}

int camkes_io_ops(ps_io_ops_t *ops) {
    assert(ops != NULL);
    return camkes_io_mapper(&ops->io_mapper) ||
           camkes_io_port_ops(&ops->io_port_ops) ||
           camkes_dma_manager(&ops->dma_manager);
}
