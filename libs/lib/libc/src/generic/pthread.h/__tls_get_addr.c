#include <pmos/tls.h>
#include <pthread.h>
#include <stdlib.h>

void *__get_tp();
int resize_dtv(struct uthread *self, size_t index) { 
    const size_t entry_size = sizeof(self->dtv[0]);

    const size_t old_size = self->dtv_size;
    const size_t new_size = index + 1;

    void *new_dtv = realloc(self->dtv, new_size * entry_size);
    if (new_dtv == NULL)
        return -1;

    self->dtv = new_dtv;
    for (size_t i = old_size; i < new_size; i++) {
        self->dtv[i].data_ptr = NULL;
        self->dtv[i].generation = 0;
    }

    if (old_size == 0) {
        // Set TLS data
        self->dtv[0].data_ptr = __get_tp();
    }

    return 0;
}

size_t get_dtv_generation(size_t) { return 0; }

void *dtv_allocate_get_addr(size_t *) { return NULL; }

#ifdef __riscv
#define TLS_DTV_OFFSET 0x800
#else
#define TLS_DTV_OFFSET 0
#endif

void *__tls_get_addr(size_t *v)
{
    const size_t object = v[0];
    const size_t offset = v[1];
    struct uthread *self = __get_tls();

    // if ((self->dtv_size < object) && (resize_dtv(self, object) < 0))
    //     return NULL;

    // const size_t generation = get_dtv_generation(object);

    // if (self->dtv[object].generation != generation)
    //     return dtv_allocate_get_addr(v);

    // Since dynamic linking is not yet implemented...
    return (char *)__get_tp() + offset + TLS_DTV_OFFSET;
}