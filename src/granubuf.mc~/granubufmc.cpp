// if no window is set, default cosine wave, with x y curve function

#include "ext.h"
#include "ext_obex.h"
#include "ext_globalsymbol.h"
#include "buffer.h"
#include "jpatcher_utils.h"

#include "buffer_proxy.hpp"
#include "grain.hpp"


#include <string>

#define GRANUMC_MAX_BUFFERS 256
#define GRANUMC_NUMINLETS 7

typedef enum _granubuf_in
{
    TRIGGER,
    DURATION,
    RATE,
    STARTLOCATION,
    BUF_INDEX,
    WIND_INDEX, // 1 int/float: window_index, length 2: interpolation between two windows, with weights
    AMPLIST // length determines number of outputs
} eGranuInlets;

typedef enum _granu_interp
{
    NONE,
    LINEAR,
    CUBIC
} eGInterp;

static t_class *granubuf_mc_class = NULL;

typedef  std::vector<GranuGrain>    GrainVec;
typedef  std::vector<t_buffer_proxy*> BufRefVec;

typedef struct _granuMC
{
    t_pxobject  m_obj;
    GrainVec    grains;
    
    t_symbol *  polybuffer_name; // maybe make it so there's only one buffer, or one polybuffer
    t_object *  polybuffer_obj = NULL;
    
    BufRefVec   buf_refs;
    long        numbufs;
    bool        buf_soft_lock;
    
    t_symbol *  buf_name[GRANUMC_MAX_BUFFERS];

    long        src_channels;
    long        channel_offset;
    
    BufRefVec   wind_refs;
    long        numwind_bufs;
    t_symbol *  wind_buf_name[GRANUMC_MAX_BUFFERS];

    t_symbol *  window_mode_attr; // beta (default), cos, or names
    long        window_mode;
    
    long        interp_type;

    long        busy;
    
    float       samplerate;
    float       sampleinterval;

    double      ms2samps;
    
    long        numoutputs;
    
    long        totalIns;
    long        isConnected[GRANUMC_NUMINLETS];
    long        inputChannels[GRANUMC_NUMINLETS];
    long        startsAt[GRANUMC_NUMINLETS];
    
    long        num_voices;
    
    long        loopmode;
    
    t_critical  lock;

} t_granu_mc;


void granu_mc_perform64(t_granu_mc *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
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
    
  //  critical_enter(x->lock);
    bool loopmode = x->loopmode > 0;
    auto& grains = x->grains;
    long numbufs = x->numbufs;
    //auto buf_refs = x->buf_refs;
  //  printf("dsp numbufs %ld\n", numbufs);

    long numvoices = x->num_voices;
    long busy = x->busy;
    long numGrainOuts = x->numoutputs;
    double ** trigger = &ins[x->startsAt[TRIGGER]];
    double ** start = &ins[x->startsAt[STARTLOCATION]];
    
    double ** rate = &ins[x->startsAt[RATE]];
    bool useDefaultRate = !x->isConnected[RATE];

    double ** dur = &ins[x->startsAt[DURATION]];
    bool useDefaultDur = !x->isConnected[DURATION];

    double ** bufIdx = &ins[x->startsAt[BUF_INDEX]];
    double ** winIdx = &ins[x->startsAt[WIND_INDEX]];
    
    double ** amps = &ins[x->startsAt[AMPLIST]];
    bool useDefaultAmp = !x->isConnected[AMPLIST];
    
    long num_amps = x->inputChannels[AMPLIST];
    
    long num_win_coef = x->inputChannels[WIND_INDEX];

    long windowMode = x->window_mode;
    
    std::vector<double> ampvec(numGrainOuts, sizeof(double));
    std::vector<double> windcoef(num_win_coef, sizeof(double));
    
    double maxAmp, busyCount, _dur, _rate;

    //windcoef.resize(num_win_coef);
    //ampvec.resize(numGrainOuts);
    
//    post("gran %ld total %ld numins %ld", numGrainOuts, numouts, numins);
    //long n_in_channels;
    //long n_in_start;


        
    t_bool alloccheck = false;
    
    t_buffer_obj    *buffer[numbufs];
    t_float         *bufferData[numbufs];

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
                    x->buf_refs[i]->rate_scalar = buf_sr[i] / x->samplerate;
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
        
        _dur = useDefaultDur ? -1 : dur[0][k];
        _rate = useDefaultRate ? 1 : rate[0][k];
        
        for( int j = 0; j < numGrainOuts; j++)
        {
            outs[j][k] = 0.0;
            if( j < num_amps || useDefaultAmp )
            {
                ampvec[j] = useDefaultAmp ? 1.0f : amps[j][k];
                maxAmp = ampvec[j] > maxAmp ? ampvec[j] : maxAmp;
            }
            
        }
        
        if( numbufs )
        {
            _bufIdx = (int)CLAMP(bufIdx[0][k], 0, numbufs-1);

            for( long i = 0; i < numvoices; i++ )
            {
                
                if(!alloccheck &&
                   trigger[0][k] > 0 &&
                   busy < numvoices && maxAmp > 0 &&
                   buf_valid[ _bufIdx ] &&
                   _dur != 0 &&
                   _rate != 0 )
                {
                    
                    if( !grains[i].m_active )
                    {
                        
                        for( int j = 0; j < num_win_coef; j++){
                            windcoef[j] = winIdx[j][k];
                        }
                        
//                        post("src channels %ld", x->src_channels);

                        grains[i].set(start[0][k],
                                      _dur * x->ms2samps,
                                      _rate,
                                      _bufIdx,
                                      windcoef,
                                      ampvec,
                                      x->buf_refs[_bufIdx],
                                      NULL, // future holder of optional window buffer
                                      loopmode,
                                      windowMode,
                                      x->channel_offset,
                                      x->src_channels );
                        
                        alloccheck = true;
                    }
                    
                }
                
                _g_bufIdx = (int)CLAMP(grains[i].m_buf_index, 0, numbufs-1);
                
                if( grains[i].m_active && buf_valid[ _g_bufIdx ] && grains[i].m_buf_len <= buf_len[_g_bufIdx] )
                {
                    
                    std::vector<double> outSamps;
                    
                    if( x->src_channels > 1 )
                        outSamps = grains[i].incr_src_channels( bufferData[ _g_bufIdx ], x->interp_type );
                    else
                        outSamps = grains[i].incr( bufferData[ _g_bufIdx ], x->interp_type );
                    
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

void granu_mc_getStartPoints(t_granu_mc *x)
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


void granu_mc_clear(t_granu_mc *x)
{
    //critical_enter(x->lock);
    for( long i = 0; i < x->grains.size(); i++)
        x->grains[i].reset();
    //critical_exit(x->lock);
}

void granu_mc_dsp64(t_granu_mc *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    
    for( long i = 0; i < GRANUMC_NUMINLETS; i++)
    {
        long n = (long)object_method(dsp64, gensym("getnuminputchannels"), x, i);
        x->isConnected[i] = count[i];
        x->inputChannels[i] = n;
        
        if( x->src_channels == 1 && i == AMPLIST && x->isConnected[i] )
        {
            x->numoutputs = n;
        }
    }
    
    if( x->src_channels > 1 )
        x->numoutputs = x->src_channels;
    
    
    granu_mc_getStartPoints(x);
    
    x->samplerate = samplerate;
    x->sampleinterval = 1.0 / x->samplerate;
    x->ms2samps = x->samplerate * 0.001;
    
    granu_mc_clear(x);
    
    dsp_add64(dsp64, (t_object*)x, (t_perfroutine64)granu_mc_perform64, 0, NULL);

}



t_max_err granu_mc_buf_set(t_granu_mc *x, t_object *attr, long argc, t_atom *argv)
{
    granu_mc_clear(x);
    
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
        else
        {
            object_error((t_object *)x, "max buffer count is %d", GRANUMC_MAX_BUFFERS );
        }
    }
    
    return 0;
}

t_max_err granu_mc_buf_get(t_granu_mc *x, t_object *attr, long *argc, t_atom **argv)
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

void granu_mc_polybuf_append(t_granu_mc *x, long newsize)
{
    if( newsize < GRANUMC_MAX_BUFFERS )
    {
        long currentCount = x->numbufs;

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

        x->numbufs = newsize;

    }
    else
    {
        object_error((t_object *)x, "max buffer count is %d", GRANUMC_MAX_BUFFERS );
    }
    

}


void granu_mc_buf_clear(t_granu_mc *x)
{
    
    granu_mc_clear(x);

    for( int i = 0 ; i < x->buf_refs.size(); i++ )
    {
        buffer_proxy_free(x->buf_refs[i]);
        object_free(x->buf_refs[i]);
    }
    
    x->buf_refs.clear();
 //   post("clear size %ld", x->buf_refs.size());
    
    x->numbufs = 0;

}


t_max_err granu_mc_polybuffer_attach_notify(t_granu_mc *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    
    if (msg == gensym("count_changed"))
    {
        long count = object_attr_getlong(data, _sym_count);

     //   if (x->polybuffer_name && x->polybuffer_name->s_name)
     //       printf("polybuffer_tester is notified, that %s now holds %ld buffers.\n", x->polybuffer_name->s_name, count);

        critical_enter(x->lock);
        if( count != x->numbufs )
        {
            if( count != 0 )
                granu_mc_polybuf_append(x, count);
            else
                granu_mc_buf_clear(x);
        }
       
        x->buf_soft_lock = false;
        
        critical_exit(x->lock);
        //printf("unlocked.\n\n");
        
        
    } else if (msg == gensym("polybuffer_free")) {
        t_symbol *name = object_attr_getsym(data, _sym_name);
        
        if (name && name->s_name && name == x->polybuffer_name) {
       //     object_post((t_object *)x, "polybuffer_tester is notified that %s is being freed.", x->polybuffer_name->s_name);
            
            critical_enter(x->lock);
            granu_mc_buf_clear(x);
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
        granu_mc_buf_clear(x);
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

void granu_mc_polybuffer_doset(t_granu_mc *x, t_symbol *s, short ac, t_atom *av)
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
                granu_mc_buf_clear(x);

            long count = object_attr_getlong(x->polybuffer_obj, _sym_count);
            granu_mc_polybuf_append(x, count);
            
            
        } else {
            object_error((t_object *)x, "%s is not a valid polybuffer", s->s_name);
            return;
        }
     
        
        if (old_name && old_name->s_name &&  old_name != x->polybuffer_name)
            globalsymbol_dereference((t_object *)x, old_name->s_name, "polybuffer");
    }
}

t_max_err granu_mc_polybuf_set(t_granu_mc *x, t_object *attr, long argc, t_atom *argv)
{
    
    if(argc == 1 && atom_gettype(argv) == A_SYM)
    {
        granu_mc_polybuffer_doset(x, atom_getsym(argv), 0, NULL );
    }
    else
    {
        object_error((t_object *)x, "polybuffer name must be a string");
    }
    
    return 0;
}


t_max_err granu_mc_polybuf_get(t_granu_mc *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setsym(*argv, x->polybuffer_name);
    return 0;
}



t_max_err granu_mc_interp_set(t_granu_mc *x, t_object *attr, long argc, t_atom *argv)
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


t_max_err granu_mc_interp_get(t_granu_mc *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setlong(*argv, x->interp_type);
    return 0;
}


t_max_err granu_mc_window_set(t_granu_mc *x, t_object *attr, long argc, t_atom *argv)
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
            // if user window is set, then window shaping functions are disabled
            // user has to shape the buffer itself if they want to manipulate it
            object_error((t_object *)x, "not yet ready to use external window buffers");
        }
        //granu_mc_polybuffer_doset(x, atom_getsym(argv), 0, NULL );
    }
    else
    {
        object_error((t_object *)x, "polybuffer name must be a string");
    }
    
    return 0;
}


t_max_err granu_mc_window_get(t_granu_mc *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setsym(*argv, x->window_mode_attr);
    return 0;
}


long granu_mc_multichanneloutputs(t_granu_mc *x, long outletindex)
{
    //post("n outs %ld", x->numoutputs);
    if( outletindex == 0)
        return x->numoutputs;//x->numoutputs;
    else
        return 1;
}


long granu_mc_inputchanged(t_granu_mc *x, long index, long count)
{
    
    if( count != x->inputChannels[index] )
    {
        critical_enter(x->lock);
        x->inputChannels[index] = count;
        granu_mc_getStartPoints(x);
        if( index == AMPLIST && count != x->numoutputs )
        {
            if( x->src_channels > 1 )
                x->numoutputs = x->src_channels;
            else
                x->numoutputs = x->inputChannels[AMPLIST];
            
            critical_exit(x->lock);
            return true;
        }
        
        critical_exit(x->lock);
    }
    
    return false;
}

void granu_mc_assist(t_granu_mc *x, void *b, long m, long a, char *s)
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
                sprintf(s, "(signal/int) window coefs a/b"); //  (to do: add window buffer index)
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


void granu_mc_free(t_granu_mc *x)
{
    dsp_free((t_pxobject *)x);
    
    granu_mc_buf_clear(x);
    
    x->grains.clear();

    critical_free(x->lock);
    
    if( x->polybuffer_name && x->polybuffer_name->s_name )
        globalsymbol_dereference((t_object *)x, x->polybuffer_name->s_name, "polybuffer");

    
}


void *granu_mc_new(t_symbol *s, long argc, t_atom *argv)
{
    t_granu_mc *x = (t_granu_mc *)object_alloc(granubuf_mc_class);
    
    if (x) {
        
        x->num_voices = 256;
        x->busy = 0;
        
        x->samplerate =  sys_getsr();
        if(x->samplerate <= 0)
            x->samplerate = 44100;
        
        x->ms2samps = x->samplerate * 0.001;
        x->sampleinterval = 1.0 / x->samplerate;
     
        x->loopmode = true;
        x->interp_type = 2;
        
        x->polybuffer_obj = NULL;
        x->polybuffer_name = NULL;
        
        if( argc && atom_gettype(argv) == A_SYM)
        {
            x->polybuffer_name = atom_getsym(argv);
        }
        
        x->window_mode = 0;
        x->window_mode_attr = gensym("beta");
        
        x->channel_offset = 0;
        x->src_channels = 1;
        
        t_dictionary *d = dictionary_new();
        if (d) {
            attr_args_dictionary(d, argc, argv);
            attr_dictionary_process(x, d); //calls appropriate class_attr_accessors
            object_free(d);
        }
     
        
        x->grains.resize( x->num_voices );
        
        dsp_setup((t_pxobject *)x, GRANUMC_NUMINLETS);
        x->m_obj.z_misc = Z_NO_INPLACE | Z_MC_INLETS;

        outlet_new(x, "signal");
        outlet_new(x, "multichannelsignal");
        
        critical_new(&x->lock);

        if( x->polybuffer_name ) {
         //   post("%p trying to connect to %s", x, x->polybuffer_name->s_name);
            defer_low(x, (method)granu_mc_polybuffer_doset, x->polybuffer_name, 0, NULL);
        }
        
    }
    
    return x;
}


BEGIN_USING_C_LINKAGE

int C74_EXPORT main(void)
{
    common_symbols_init();

    t_class *c = class_new("granubuf.mc~",
                           (method)granu_mc_new,
                           (method)granu_mc_free,
                           (long)sizeof(t_granu_mc),
                           (method)NULL,
                           A_GIMME,
                           0L);
    
    class_addmethod(c, (method)granu_mc_dsp64,  "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)granu_mc_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)granu_mc_inputchanged, "inputchanged", A_CANT, 0);
    class_addmethod(c, (method)granu_mc_assist, "assist",    A_CANT, 0);
    class_addmethod(c, (method)granu_mc_clear, "clear", 0);
    
    class_addmethod(c, (method)granu_mc_polybuffer_attach_notify, "notify", A_CANT, 0);

    
    CLASS_ATTR_SYM_VARSIZE(c, "buffer", 0, t_granu_mc, buf_name, numbufs, GRANUMC_MAX_BUFFERS);
    CLASS_ATTR_ACCESSORS(c, "buffer", granu_mc_buf_get, granu_mc_buf_set);
    CLASS_ATTR_LABEL(c, "buffer", 0, "buffer list");
    
    CLASS_ATTR_SYM(c, "polybuffer", 0, t_granu_mc, polybuffer_name);
    CLASS_ATTR_ACCESSORS(c, "polybuffer", granu_mc_polybuf_get, granu_mc_polybuf_set);
    CLASS_ATTR_LABEL(c, "polybuffer", 0, "linked polybuffer");

    CLASS_ATTR_SYM(c, "window", 0, t_granu_mc, window_mode_attr);
    CLASS_ATTR_ACCESSORS(c, "window", granu_mc_window_get, granu_mc_window_set);
   // CLASS_ATTR_ENUMINDEX2(c, "window", 0, "beta", "cos");

    CLASS_ATTR_INT32(c, "interpolation", 0, t_granu_mc, interp_type);
    CLASS_ATTR_ACCESSORS(c, "interpolation", granu_mc_interp_get, granu_mc_interp_set);
    CLASS_ATTR_ENUMINDEX3(c, "interpolation", 0, "none", "linear", "cubic");
    
    CLASS_ATTR_LONG(c, "loop", 0, t_granu_mc, loopmode);
    CLASS_ATTR_STYLE_LABEL(c, "loop", 0, "onoff", "loop mode");
    
    CLASS_ATTR_LONG(c, "src_channels", 0, t_granu_mc, src_channels);
    CLASS_ATTR_FILTER_MIN(c, "src_channels", 1);

    CLASS_ATTR_LONG(c, "src_channel_offset", 0, t_granu_mc, channel_offset);
    CLASS_ATTR_FILTER_MIN(c, "src_channel_offset", 0);

    CLASS_ATTR_LONG(c, "num_voices", 0, t_granu_mc, num_voices);
    CLASS_ATTR_FILTER_CLIP(c, "num_voices", 1, 256);
    CLASS_ATTR_LABEL(c, "num_voices", 0, "max number of voices");

    buffer_proxy_init();
    
    granubuf_mc_class = c;
    class_register(CLASS_BOX, granubuf_mc_class);

    class_dspinit(c);

    return 0;
}
END_USING_C_LINKAGE
