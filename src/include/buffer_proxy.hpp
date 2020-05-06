#pragma once

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "z_dsp.h"
#include "buffer.h"

typedef struct _buffer_proxy
{
    t_object        ob;
    t_buffer_ref    *ref;
    t_symbol        *name;
    t_bool          buffer_modified;
    double          sr;
    double          nframes;
    long            nchans;
    double          rate_scalar;
    
} t_buffer_proxy;


t_max_err buffer_proxy_notify(t_buffer_proxy *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
void buffer_proxy_set_ref(t_buffer_proxy *x, t_symbol *s);
void buffer_proxy_free(t_buffer_proxy *x);
void buffer_proxy_markRead(t_buffer_proxy *x);
t_buffer_proxy *buffer_proxy_new(t_symbol *s);
void buffer_proxy_init();
