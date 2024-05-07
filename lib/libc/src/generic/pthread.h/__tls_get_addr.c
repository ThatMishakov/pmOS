#include <pthread.h>
#include <pmos/tls.h>

int resize_dtv()
{
    return -1;
}

size_t get_dtv_generation(size_t index)
{
    return -1;
}


void *dtv_allocate_get_addr(size_t *)
{
    return NULL;
}

// void *__tls_get_addr(size_t *v)
// {
//     const size_t object = v[0];
//     const size_t offset = v[1];
//     const struct uthread *self = __get_tls();
    
//     if ((self->dtv_size < object) && (resize_dtv() < 0))
//         return NULL;

//     const size_t generation = get_dtv_generation(object);

//     if (self->dtv[object].generation != generation)
//         return dtv_allocate_get_addr(v);

//     return (void *)((size_t)self->dtv[0].data_ptr + offset);
// }