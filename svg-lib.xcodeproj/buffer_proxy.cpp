
#include "buffer_proxy.hpp"

static t_class *buffer_proxy_class = NULL;
static t_symbol *ps_buffer_modified = NULL;

t_max_err buffer_proxy_notify(t_buffer_proxy *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    if (msg == ps_buffer_modified)
        x->buffer_modified = true;
    
    //post("s %s msg %s", s->s_name, msg->s_name);
    
    return buffer_ref_notify(x->ref, s, msg, sender, data);
}

void buffer_proxy_set_ref(t_buffer_proxy *x, t_symbol *s)
{
    x->name = s;
    buffer_ref_set(x->ref, x->name);
    x->buffer_modified = true;
}

void buffer_proxy_free(t_buffer_proxy *x)
{
    if( x->ref )
        object_free(x->ref);
    
}

void buffer_proxy_markRead(t_buffer_proxy *x)
{
    x->buffer_modified = false;
}

t_buffer_proxy *buffer_proxy_new(t_symbol *s)
{
    t_buffer_proxy *x = NULL;
    
    if((x = (t_buffer_proxy *)object_alloc(buffer_proxy_class)))
    {
        x->name = s;
        x->ref = buffer_ref_new((t_object *)x, x->name);
        if( !x->ref )
        {
            object_error((t_object *)x, "failed to attach to buffer %s", s->s_name);
            object_free(x);
            return NULL;
        }
        
        t_buffer_obj * buf = buffer_ref_getobject(x->ref);
        
        x->nframes = buffer_getframecount(buf);
        x->nchans = buffer_getchannelcount(buf);
        x->sr = buffer_getsamplerate(buf);
        
        x->buffer_modified = false;
        
    }
    
    return x;
}

void buffer_proxy_init()
{
    t_class *bufpxy = class_new("bufferproxy", NULL, NULL, sizeof(t_buffer_proxy), 0L, 0);
    class_addmethod(bufpxy, (method)buffer_proxy_notify, "notify",    A_CANT, 0);
    class_register(CLASS_NOBOX, bufpxy);
    buffer_proxy_class = bufpxy;
    ps_buffer_modified = gensym("buffer_modified");
}
