// if no window is set, default cosine wave, with x y curve function

#include "ext.h"
#include "ext_obex.h"
#include "ext_globalsymbol.h"
#include "buffer.h"
#include "jpatcher_utils.h"

#include "buffer_proxy.hpp"

#include <string>


#define POLYWAVE_MAX_BUFFERS 2048

typedef enum _polywave_interp
{
    NONE,
    LINEAR,
    CUBIC
} t_polywave_interp;

typedef enum _polywave_dims
{
    ZERO,
    ONE_D,
    TWO_D
} t_polywave_dims;



static t_class *mc_polywave_class = NULL;

typedef  std::vector<t_buffer_proxy*> BufRefVec;

typedef struct _granuMC
{
    t_pxobject  m_obj;
        
    t_symbol *  polybuffer_name; // maybe make it so there's only one buffer, or one polybuffer
    t_object *  polybuffer_obj = NULL;
    
    BufRefVec   buf_refs;
    long        numbufs;
    bool        buf_soft_lock;
    
    t_symbol *  buf_name[GRANUMC_MAX_BUFFERS];

    long        busy;
    
    float       samplerate;
    float       sampleinterval;

    long        numoutputs;
    
    t_polywave_interp   interp_type;
    long                dims;
    
    
    long        totalIns;
    long        isConnected[GRANUMC_NUMINLETS];
    long        inputChannels[GRANUMC_NUMINLETS];
    long        startsAt[GRANUMC_NUMINLETS];
            
    t_critical  lock;

} t_mc_polywave;


void mc_polywave_perform64(t_mc_polywave *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    
    if( x->buf_soft_lock )
    {
        for ( long k = 0; k < sampleframes; k++)
        {
            for(long i = 0; i < numouts; i++)
                outs[i][k] = 0.0;
            
        }
        
        return;
    }
    
    int i;
    t_double        *out = outs[0];
    t_double        *in1 = ins[0];
    t_double        *in2 = ins[1];
    int    n = sampleframes;

    if(x->numbufs == 0 || !x->w_connected[0])
    {
        while (n--)
            *out++ = 0.;
        return;
    }

    int             idx_connected = x->w_connected[1];
    long            numbufs = x->numbufs;
    long            frames[numbufs], nchans[numbufs];

    t_buffer_obj    *buffer[numbufs];
    t_float         *tab[numbufs];
    int             valid[numbufs], modified[numbufs];
    
        
    long buf_len[numbufs], buf_nchans[numbufs], buf_valid[numbufs], buf_sr[numbufs];

    long _bufIdx, _g_bufIdx ;
    
    for (long i = 0; i < numbufs; i++)
    {
        buffer[i] = buffer_ref_getobject(x->buf_refs[i]->ref);
        
        if( !buffer[i] )
            buf_valid[i] = 0;
        else
        {
            bufferData[i] = buffer_locksamples(buffer[i]);
            
           
            if(!bufferData[i])
                buf_valid[i] = 0;
            else
            {
                
                if(x->buf_refs[i]->buffer_modified)
                {
                    buf_len[i] = buffer_getframecount(buffer[i]);
                    buf_nchans[i] = buffer_getchannelcount(buffer[i]);
                    buf_sr[i] = buffer_getsamplerate(buffer[i]);
                    x->buf_refs[i]->nframes = buf_len[i];
                    x->buf_refs[i]->nchans = buf_nchans[i];
                    x->buf_refs[i]->sr = buf_sr[i];
                    buffer_proxy_markRead(x->buf_refs[i]);
                }
                else
                {
                    buf_len[i] = x->buf_refs[i]->nframes;
                    buf_nchans[i] = x->buf_refs[i]->nchans;
                }
               
                buf_valid[i] = (buf_nchans[i] > 0 && buf_len[i] > 0);
                
            }
        }
    }
    
    for ( long k = 0; k < sampleframes; k++)
    {
        alloccheck = false;
        maxAmp = 0;
        busyCount = 0;
        
        for( int j = 0; j < numGrainOuts; j++)
        {
            outs[j][k] = 0.0;
            ampvec[j] = useDefaultAmp ? 1.0f : amps[j][k];
            maxAmp = ampvec[j] > maxAmp ? ampvec[j] : maxAmp;
        }
        
        if( numbufs )
        {
            _bufIdx = (int)CLAMP(bufIdx[0][k], 0, numbufs-1);

            for( long i = 0; i < numvoices; i++ )
            {
                
                if( !alloccheck && trigger[0][k] > 0 && busy < numvoices && maxAmp > 0 && buf_valid[ _bufIdx ] && rate[0][k] && dur[0][k] )
                {
                    
                    if( !grains[i].m_active )
                    {
                        
                        for( int j = 0; j < num_win_coef; j++){
                            windcoef[j] = winIdx[j][k];
                        }
                        
                       // post("bufidx %ld %ld", _bufIdx,  buf_valid[ _bufIdx ]);

                        grains[i].set(start[0][k],
                                      dur[0][k] * x->ms2samps,
                                      rate[0][k],
                                      _bufIdx,
                                      windcoef,
                                      ampvec,
                                      x->buf_refs[_bufIdx],
                                      NULL,
                                      loopmode,
                                      windowMode );
                        
                        alloccheck = true;
                    }
                    
                }
                
                _g_bufIdx = (int)CLAMP(grains[i].m_buf_index, 0, numbufs-1);
                
                if( grains[i].m_active && buf_valid[ _g_bufIdx ] && grains[i].m_buf_len <= buf_len[_g_bufIdx] )
                {
                    
                    auto outSamps = grains[i].incr( bufferData[ _g_bufIdx ], x->interp_type );
                    
                    for( int j = 0; j < numGrainOuts; j++)
                    {
                        outs[j][k] += outSamps[j];
                    }
                    
                    busyCount++;
                }
            }
        }
        
        outs[numGrainOuts][k] = (double)busyCount;
        
    }
    
    for (long i = 0; i < numbufs; i++)
    {
        if( bufferData[i] )
            buffer_unlocksamples(buffer[i]);
    }

}

void mc_polywave_getStartPoints(t_mc_polywave *x)
{
    long i, count = 0;
    x->startsAt[0] = 0;
//    post("0 starts at 0");

    for (i = 1; i < GRANUMC_NUMINLETS; i++) {
        count += x->inputChannels[i-1];
        x->startsAt[i] = count;
//        post("%ld starts at %ld", i, x->startsAt[i]);
    }
    
    x->totalIns = count + x->inputChannels[AMPLIST];
//    post("total %ld amps %ld", x->totalIns, x->inputChannels[AMPLIST] );
}


void mc_polywave_clear(t_mc_polywave *x)
{
    //critical_enter(x->lock);
    for( long i = 0; i < x->grains.size(); i++)
        x->grains[i].reset();
    //critical_exit(x->lock);
}

void mc_polywave_dsp64(t_mc_polywave *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    for( long i = 0; i < GRANUMC_NUMINLETS; i++)
    {
        long n = (long)object_method(dsp64, gensym("getnuminputchannels"), x, i);
        x->isConnected[i] = count[i];
        x->inputChannels[i] = n;
        
        if( i == AMPLIST ){
            x->numoutputs = n;
        }
    }
    mc_polywave_getStartPoints(x);
    
    x->samplerate = sys_getsr();
    x->sampleinterval = 1.0 / x->samplerate;
    x->ms2samps = x->samplerate * 0.001;
    
    mc_polywave_clear(x);
    
    dsp_add64(dsp64, (t_object*)x, (t_perfroutine64)mc_polywave_perform64, 0, NULL);

}



t_max_err mc_polywave_buf_set(t_mc_polywave *x, t_object *attr, long argc, t_atom *argv)
{
    mc_polywave_clear(x);
    
    t_symbol *s;
    
    if(argc && argv)
    {
        if(argc < GRANUMC_MAX_BUFFERS)
        {
            critical_enter(x->lock);
            
            size_t currentSize = x->buf_refs.size();
            
            if( argc > x->buf_refs.size() )
                x->buf_refs.reserve(argc);
            
            long i = 0;
            while( i < argc)
            {
                if(atom_gettype(argv+i) != A_SYM)
                {
                    object_error((t_object *)x, "non symbol buffer name");
                    critical_exit(x->lock);
                    return -1;
                }
                s = atom_getsym(argv+i);
                
                
                if(s) {
                    
                    if( i < currentSize )
                        buffer_proxy_set_ref(x->buf_refs[i], s);
                    else
                        x->buf_refs.emplace_back( buffer_proxy_new(s) );
                    
                    x->buf_name[i] = s;

                } else {
                    object_error((t_object *)x, "must have buffer name");
                }
                i++;
            }
                
            
            x->numbufs = argc;
            critical_exit(x->lock);
        }
    }
    
    return 0;
}

t_max_err mc_polywave_buf_get(t_mc_polywave *x, t_object *attr, long *argc, t_atom **argv)
{
    char alloc;
    int i;
  //  post("get called num %ld", x->numbufs);

    atom_alloc_array(x->numbufs, argc, argv, &alloc);
    
    if (x->numbufs)
    {
        for(i = 0; i < x->numbufs; i++)
        {
          //  post("setting buffer name %s", x->buf_name[i]->s_name);
            atom_setsym(*argv+i, x->buf_name[i]);
        }
    }
    else
    {
        atom_setsym(*argv, gensym("<empty>"));
    }
    
    return 0;
}

void mc_polywave_polybuf_append(t_mc_polywave *x, long newsize)
{
    long currentCount = x->numbufs;
    if( newsize < GRANUMC_MAX_BUFFERS )
    {
     //   post("pre size %ld", x->buf_refs.size());
        if( newsize > x->buf_refs.size() )
            x->buf_refs.reserve(newsize);
        
//        post("post size %ld", x->buf_refs.size());
        std::string basename = x->polybuffer_name->s_name;
        t_symbol *s = NULL;
        
        for( long i = currentCount; i < newsize; i++)
        {
            s = gensym( (basename + "." + std::to_string(i+1)).c_str() );
            x->buf_refs.emplace_back( buffer_proxy_new(s) );
            x->buf_name[i] = s;
        }
    }
    
    x->numbufs = newsize;

}


void mc_polywave_buf_clear(t_mc_polywave *x)
{
    
    mc_polywave_clear(x);

    for( int i = 0 ; i < x->buf_refs.size(); i++ )
    {
        buffer_proxy_free(x->buf_refs[i]);
        object_free(x->buf_refs[i]);
    }
    
    x->buf_refs.clear();
   // post("clear size %ld", x->buf_refs.size());
    
    x->numbufs = 0;

}


t_max_err mc_polywave_polybuffer_attach_notify(t_mc_polywave *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    
    if (msg == gensym("count_changed"))
    {
        long count = object_attr_getlong(data, _sym_count);
/*
        if (x->polybuffer_name && x->polybuffer_name->s_name)
            printf("polybuffer_tester is notified, that %s now holds %ld buffers.\n", x->polybuffer_name->s_name, count);
  */
        critical_enter(x->lock);
        if( count != x->numbufs )
        {
            if( count != 0 )
                mc_polywave_polybuf_append(x, count);
            else
                mc_polywave_buf_clear(x);
        }
       
        x->buf_soft_lock = false;
        
        critical_exit(x->lock);
        //printf("unlocked.\n\n");
        
        
    } else if (msg == gensym("polybuffer_free")) {
        t_symbol *name = object_attr_getsym(data, _sym_name);
        
        if (name && name->s_name && name == x->polybuffer_name) {
       //     object_post((t_object *)x, "polybuffer_tester is notified that %s is being freed.", x->polybuffer_name->s_name);
            
            critical_enter(x->lock);
            mc_polywave_buf_clear(x);
            critical_exit(x->lock);

            globalsymbol_dereference((t_object *)x, x->polybuffer_name->s_name, "polybuffer");    // detaching
            x->polybuffer_obj = NULL;
            x->polybuffer_name = NULL;
        }
    } else if (msg == gensym("child_modified")) {
/*
        t_buffer *b = (t_buffer *)data;
        if (b->b_name && b->b_name->s_name)
            printf("buffer %s was modified\n", b->b_name->s_name);
*/
    }
    else if (msg == gensym("count_will_change"))
    {
        critical_enter(x->lock);
        x->buf_soft_lock = true;
        mc_polywave_buf_clear(x);
        critical_exit(x->lock);
//        printf("count_will_change, now locked\n");

        /*
        object_post((t_object *)x, "polybuffer_tester is notified, that the number of buffers hold by %s is about to change", x->polybuffer_name->s_name);
        */
    }
    else
    {
        printf("polybuffer other msg %s\n", msg->s_name);
    }
    
    return MAX_ERR_NONE;
}   

long polybuffer_symbol_is_polybuffer(t_symbol *s)
{
    if (s && s->s_thing && !NOGOOD(s->s_thing) && ob_sym(s->s_thing) == gensym("polybuffer"))
        return (1);
    else
        return (0);
}

t_object *polybuffer_getobject(t_symbol *name)
{
    t_object *rtn = NULL;
    
    if ( polybuffer_symbol_is_polybuffer(name) )
    {
        rtn = (t_object *)(name->s_thing);
    }
    
    return (rtn);
}

void mc_polywave_polybuffer_doset(t_mc_polywave *x, t_symbol *s, short ac, t_atom *av)
{
    if (s && s->s_name && s != _sym_nothing )
    {
        t_symbol *old_name = x->polybuffer_name;
        x->polybuffer_obj = polybuffer_getobject(s);

        if ( x->polybuffer_obj )
        {
            x->polybuffer_name = s;
            globalsymbol_reference((t_object *)x, x->polybuffer_name->s_name, "polybuffer");

            if( x->numbufs > 0 )
                mc_polywave_buf_clear(x);

            long count = object_attr_getlong(x->polybuffer_obj, _sym_count);
            mc_polywave_polybuf_append(x, count);
            
            
        } else {
            object_error((t_object *)x, "%s is not a valid polybuffer", s->s_name);
            return;
        }
     
        
        if (old_name && old_name->s_name &&  old_name != x->polybuffer_name)
            globalsymbol_dereference((t_object *)x, old_name->s_name, "polybuffer");
    }
}
t_max_err mc_polywave_polybuf_set(t_mc_polywave *x, t_object *attr, long argc, t_atom *argv)
{
    
    if(argc == 1 && atom_gettype(argv) == A_SYM)
    {
      //  post("mc_polywave_polybuffer_ATTR set");
        
        mc_polywave_polybuffer_doset(x, atom_getsym(argv), 0, NULL );
    }
    else
    {
        object_error((t_object *)x, "polybuffer name must be a string");
    }
    
    return 0;
}


t_max_err mc_polywave_polybuf_get(t_mc_polywave *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setsym(*argv, x->polybuffer_name);
    return 0;
}



t_max_err mc_polywave_interp_set(t_mc_polywave *x, t_object *attr, long argc, t_atom *argv)
{
    
    if(argc == 1 && atom_gettype(argv) == A_LONG)
    {
        critical_enter(x->lock);
        x->interp_type = atom_getlong(argv);
        critical_exit(x->lock);
    } else {
        object_error((t_object *)x, "unknown interpolation ");
    }
    
    return 0;
}


t_max_err mc_polywave_interp_get(t_mc_polywave *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setlong(*argv, x->interp_type);
    return 0;
}


t_max_err mc_polywave_window_set(t_mc_polywave *x, t_object *attr, long argc, t_atom *argv)
{
    
    if(argc == 1 && atom_gettype(argv) == A_SYM)
    {
        t_symbol * mode = atom_getsym(argv);
        
        if( mode == gensym("beta"))
        {
            x->window_mode = 0;
            x->window_mode_attr = mode;
        }
        else if( mode == gensym("cos"))
        {
            x->window_mode = 1;
            x->window_mode_attr = mode;
        }
        else
        {
            object_error((t_object *)x, "not yet ready to use external window buffers");
        }
        //mc_polywave_polybuffer_doset(x, atom_getsym(argv), 0, NULL );
    }
    else
    {
        object_error((t_object *)x, "polybuffer name must be a string");
    }
    
    return 0;
}


t_max_err mc_polywave_window_get(t_mc_polywave *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setsym(*argv, x->window_mode_attr);
    return 0;
}


long mc_polywave_multichanneloutputs(t_mc_polywave *x, long outletindex)
{
    //post("n outs %ld", x->numoutputs);
    if( outletindex == 0)
        return x->numoutputs;//x->numoutputs;
    else
        return 1;
}


long mc_polywave_inputchanged(t_mc_polywave *x, long index, long count)
{
    
    if( count != x->inputChannels[index] )
    {
        critical_enter(x->lock);
        x->inputChannels[index] = count;
        mc_polywave_getStartPoints(x);
        if( index == AMPLIST && count != x->numoutputs )
        {
            x->numoutputs = x->inputChannels[AMPLIST];
            critical_exit(x->lock);
            return true;
        }
        critical_exit(x->lock);
    }
    
    return false;
}

void mc_polywave_assist(t_mc_polywave *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET ) { //inlet
        switch (a) {
            case TRIGGER:
                sprintf(s, "(signal/float) trigger != 0 ");
                break;
            case STARTLOCATION:
                sprintf(s, "(signal/float) relative sample start time 0-1 ");
                break;
            case RATE:
                sprintf(s, "(signal/float) playback rate ");
                break;
            case DURATION:
                sprintf(s, "(signal/float) grain dur ms ");
                break;
            case WIND_INDEX:
                sprintf(s, "(signal/int) window buffer index ");
                break;
            case BUF_INDEX:
                sprintf(s, "(signal/int) buffer index ");
                break;
            case AMPLIST:
                sprintf(s, "(signal/float) amplitude (default 1.0) ");
                break;
            default:
                sprintf(s, "I am inlet %ld", a);
                break;
        }
    }
    else {    // outlet
        sprintf(s, "(signal) outlet %ld", a);
    }
}


void mc_polywave_free(t_mc_polywave *x)
{
    dsp_free((t_pxobject *)x);
    
    critical_free(x->lock);
    
    if( x->polybuffer_name && x->polybuffer_name->s_name )
        globalsymbol_dereference((t_object *)x, x->polybuffer_name->s_name, "polybuffer");

    
}


void *mc_polywave_new(t_symbol *s, long argc, t_atom *argv)
{
    t_mc_polywave *x = (t_mc_polywave *)object_alloc(mc_polywave_class);
    
    if (x) {
        
        x->busy = 0;
        
        x->dims = ONE_D;
        x->interp_type = LINEAR;
        //x->backup = x->interp_type;
        
        x->numbufs = 0;
        x->polybuffer_obj = NULL;
        x->polybuffer_name = NULL;
        
        if( argc && atom_gettype(argv) == A_SYM)
        {
            x->polybuffer_name = atom_getsym(argv);
        }
        
        t_dictionary *d = dictionary_new();
        if (d) {
            attr_args_dictionary(d, argc, argv);
            attr_dictionary_process(x, d); //calls appropriate class_attr_accessors
            object_free(d);
        }
             
        int numlets = 0;
        switch (x->dims) {
            case ONE_D:
                numlets = 2;
                break;
            case TWO_D:
                numlets = 5;
                break;
            default:
                object_error((t_object *)x, "dimention attribute set to unknown value, cannont create inlets");
            break;
        }
    
        dsp_setup((t_pxobject *)x, numlets); // single dim lookup for now
        x->m_obj.z_misc = Z_NO_INPLACE | Z_MC_INLETS;

        outlet_new(x, "signal");
        outlet_new(x, "multichannelsignal");
        
        critical_new(&x->lock);

        if( x->polybuffer_name ) {
         //   post("%p trying to connect to %s", x, x->polybuffer_name->s_name);
            defer_low(x, (method)mc_polywave_polybuffer_doset, x->polybuffer_name, 0, NULL);
        }
        
    }
    
    return x;
}


BEGIN_USING_C_LINKAGE

int C74_EXPORT main(void)
{
    common_symbols_init();

    t_class *c = class_new("mc.polywave~",
                           (method)mc_polywave_new,
                           (method)mc_polywave_free,
                           (long)sizeof(t_mc_polywave),
                           (method)NULL,
                           A_GIMME,
                           0L);
    
    class_addmethod(c, (method)mc_polywave_dsp64,  "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)mc_polywave_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)mc_polywave_inputchanged, "inputchanged", A_CANT, 0);
    class_addmethod(c, (method)mc_polywave_assist, "assist",    A_CANT, 0);
    class_addmethod(c, (method)mc_polywave_clear, "clear", 0);
    
    class_addmethod(c, (method)mc_polywave_polybuffer_attach_notify, "notify", A_CANT, 0);

    
    CLASS_ATTR_SYM(c, "polybuffer", 0, t_mc_polywave, polybuffer_name);
    CLASS_ATTR_ACCESSORS(c, "polybuffer", mc_polywave_polybuf_get, mc_polywave_polybuf_set);
    CLASS_ATTR_LABEL(c, "polybuffer", 0, "linked polybuffer");

    CLASS_ATTR_INT32(c, "interpolation", 0, t_mc_polywave, interp_type);
    CLASS_ATTR_ACCESSORS(c, "interpolation", mc_polywave_interp_get, mc_polywave_interp_set);
    CLASS_ATTR_ENUMINDEX3(c, "interpolation", 0, "none", "linear", "cubic");
        
    buffer_proxy_init();
    
    mc_polywave_class = c;
    class_register(CLASS_BOX, mc_polywave_class);

    class_dspinit(c);

    return 0;
}
END_USING_C_LINKAGE
