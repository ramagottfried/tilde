// if no window is set, default cosine wave, with x y curve function

#include "ext.h"
#include "ext_obex.h"
#include "ext_globalsymbol.h"
#include "buffer.h"
#include "jpatcher_utils.h"

#include "fm_grain.hpp"


#include <string>

#define GRANUMC_MAX_BUFFERS 128
#define GRANUMC_NUMINLETS 7
#define STABSZ (1l<<16)

typedef enum _granubuf_in
{
    TRIGGER,
    DURATION,
    CAR,
    HARM,
    MOD_INDEX,
    WINDOW_SHAPE,
    AMPLIST // length determines number of outputs
} eGranuInlets;

static t_class *granufm_mc_class = NULL;

typedef  std::vector<GranuFMGrain>    GrainVec;

typedef struct _granuFM_MC
{
    t_pxobject  m_obj;
    GrainVec    grains;

    double      *sinetab;
    double      *wind_costab;

    
    bool        buf_soft_lock;
    
    long        window_mode;
    
    long        busy;
    
    float       samplerate;
    float       sampleinterval;
    double      pkw;
    
    double      ms2samps;
    
    long        numoutputs;
    
    long        totalIns;
    long        isConnected[GRANUMC_NUMINLETS];
    long        inputChannels[GRANUMC_NUMINLETS];
    long        startsAt[GRANUMC_NUMINLETS];
    
    long        num_voices;
    
    t_critical  lock;

} t_granufm_mc;


void granufm_mc_perform64(t_granufm_mc *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
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
    auto& grains = x->grains;
  //  printf("dsp numbufs %ld\n", numbufs);

    long numvoices = x->num_voices;
    long busy = x->busy;
    long numGrainOuts = x->numoutputs;
    
    double ** trigger = &ins[x->startsAt[TRIGGER]];
    double ** dur = &ins[x->startsAt[DURATION]];
    
    double ** car = &ins[x->startsAt[CAR]];
    double ** harm = &ins[x->startsAt[HARM]];
    double ** modIndex = &ins[x->startsAt[MOD_INDEX]];

    double ** winIdx = &ins[x->startsAt[WINDOW_SHAPE]];
    
    double ** amps = &ins[x->startsAt[AMPLIST]];
    bool useDefaultAmp = !x->isConnected[AMPLIST];
    
    long num_win_coef = x->inputChannels[WINDOW_SHAPE];
    
    long windowMode = x->window_mode;
    
    double sampleInterval = x->sampleinterval;
    
  //  critical_exit(x->lock);
    
    std::vector<double> ampvec, windcoef;
    double maxAmp, busyCount;

    windcoef.resize(num_win_coef);
    ampvec.resize(numGrainOuts);
    
//    post("gran %ld total %ld numins %ld", numGrainOuts, numouts, numins);
    //long n_in_channels;
    //long n_in_start;

    t_bool alloccheck = false;
  
    
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
        
       
        for( long i = 0; i < numvoices; i++ )
        {
            
            if( !alloccheck && trigger[0][k] > 0 && busy < numvoices && maxAmp > 0 && dur[0][k] )
            {
                
                if( !grains[i].m_active )
                {
                    
                    for( int j = 0; j < num_win_coef; j++){
                        windcoef[j] = winIdx[j][k];
                    }
                    
                   // post("bufidx %ld %ld", _bufIdx,  buf_valid[ _bufIdx ]);

                    grains[i].set(dur[0][k] * x->ms2samps,
                                  car[0][k] * sampleInterval, // * x->pkw, //
                                  harm[0][k],
                                  windcoef,
                                  ampvec,
                                  windowMode);
                    
                    alloccheck = true;
                }
                
            }
            
            
            if( grains[i].m_active )
            {
                
                auto outSamps = grains[i].incr( modIndex[0][k] );
                
                for( int j = 0; j < numGrainOuts; j++)
                {
                    outs[j][k] += outSamps[j];
                }
                
                busyCount++;
            }
        }

        outs[numGrainOuts][k] = (double)busyCount;
        
    }
   
}

void granufm_mc_getStartPoints(t_granufm_mc *x)
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


void granufm_mc_clear(t_granufm_mc *x)
{
    //critical_enter(x->lock);
    for( long i = 0; i < x->grains.size(); i++)
        x->grains[i].reset();
    //critical_exit(x->lock);
}

void granufm_mc_dsp64(t_granufm_mc *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
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
    granufm_mc_getStartPoints(x);
    
    x->samplerate = sys_getsr();
    x->sampleinterval = 1.0 / x->samplerate;
    x->ms2samps = x->samplerate * 0.001;
 //   x->pkw = ( STABSZ * x->sampleinterval ) ;
    
    granufm_mc_clear(x);
    
    dsp_add64(dsp64, (t_object*)x, (t_perfroutine64)granufm_mc_perform64, 0, NULL);

}



void granufm_mc_buf_clear(t_granufm_mc *x)
{
    
    granufm_mc_clear(x);

}


t_max_err granufm_mc_window_set(t_granufm_mc *x, t_object *attr, long argc, t_atom *argv)
{
    
    if(argc == 1 && atom_gettype(argv) == A_LONG)
    {
        critical_enter(x->lock);
        x->window_mode = atom_getlong(argv);
        critical_exit(x->lock);
    } else {
        object_error((t_object *)x, "unknown number ");
    }
    
    return 0;
}


t_max_err granufm_mc_window_get(t_granufm_mc *x, t_object *attr, long *argc, t_atom **argv)
{
    
    char alloc;
    atom_alloc(argc, argv, &alloc);
    atom_setlong(*argv, x->window_mode);
    return 0;
}


long granufm_mc_multichanneloutputs(t_granufm_mc *x, long outletindex)
{
    //post("n outs %ld", x->numoutputs);
    if( outletindex == 0)
        return x->numoutputs;//x->numoutputs;
    else
        return 1;
}


long granufm_mc_inputchanged(t_granufm_mc *x, long index, long count)
{
    
    if( count != x->inputChannels[index] )
    {
        critical_enter(x->lock);
        x->inputChannels[index] = count;
        granufm_mc_getStartPoints(x);
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

void granufm_mc_assist(t_granufm_mc *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET ) { //inlet
        switch (a) {
            case TRIGGER:
                sprintf(s, "(signal/float) trigger != 0 ");
                break;
            case CAR:
                sprintf(s, "(signal/float) carrier in Hz ");
                break;
            case HARM:
                sprintf(s, "(signal/float) harmonisity ratio ");
                break;
            case MOD_INDEX:
                sprintf(s, "(signal/float) mod index ");
                break;
            case DURATION:
                sprintf(s, "(signal/float) grain dur ms ");
                break;
            case WINDOW_SHAPE:
                sprintf(s, "(signal/int) window shaping ");
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


void SineFunction(int n, double *stab, int stride, double from, double to)
{
    int j;
    double f = (to-from) / n;
    
    for( j=0; j<n; ++j )
    {
        stab[ j * stride ] = sin( from + j * f );
        
    }
}

void Makeoscsinetable(t_granufm_mc *x)
{
    SineFunction(STABSZ, x->sinetab, 1, 0.0, 2.0 * PI);
}


void granufm_mc_free(t_granufm_mc *x)
{
    dsp_free((t_pxobject *)x);
    
    granufm_mc_buf_clear(x);
    
    x->grains.clear();

    critical_free(x->lock);
    
//    free(x->sinetab);
  
}


void *granufm_mc_new(t_symbol *s, long argc, t_atom *argv)
{
    t_granufm_mc *x = (t_granufm_mc *)object_alloc(granufm_mc_class);
    
    if (x) {
        
        x->num_voices = 32;
        x->busy = 0;
        
        x->samplerate =  sys_getsr();
        if(x->samplerate <= 0)
            x->samplerate = 44100;
        
        x->ms2samps = x->samplerate * 0.001;
        x->sampleinterval = 1.0 / x->samplerate;
    //    x->pkw = ( STABSZ * x->sampleinterval ) ;

        x->window_mode = 0;
        
        t_dictionary *d = dictionary_new();
        if (d) {
            attr_args_dictionary(d, argc, argv);
            attr_dictionary_process(x, d); //calls appropriate class_attr_accessors
            object_free(d);
        }
     
        
     //   x->sinetab = (double *)calloc(STABSZ, sizeof(double));
//        Makeoscsinetable(x);
        
        
        x->grains.resize( x->num_voices );
        
        dsp_setup((t_pxobject *)x, GRANUMC_NUMINLETS);
        x->m_obj.z_misc = Z_NO_INPLACE | Z_MC_INLETS;

        outlet_new(x, "signal");
        outlet_new(x, "multichannelsignal");
        
        critical_new(&x->lock);

        
    }
    
    return x;
}


BEGIN_USING_C_LINKAGE

int C74_EXPORT main(void)
{
    common_symbols_init();

    t_class *c = class_new("granufm.mc~",
                           (method)granufm_mc_new,
                           (method)granufm_mc_free,
                           (long)sizeof(t_granufm_mc),
                           (method)NULL,
                           A_GIMME,
                           0L);
    
    class_addmethod(c, (method)granufm_mc_dsp64,  "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)granufm_mc_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)granufm_mc_inputchanged, "inputchanged", A_CANT, 0);
    class_addmethod(c, (method)granufm_mc_assist, "assist",    A_CANT, 0);
    class_addmethod(c, (method)granufm_mc_clear, "clear", 0);
    
    CLASS_ATTR_LONG(c, "window", 0, t_granufm_mc, window_mode);
//    CLASS_ATTR_ACCESSORS(c, "window", granufm_mc_window_get, granufm_mc_window_set);
//    CLASS_ATTR_LABEL(c, "window", 0, "window");
    
    granufm_mc_class = c;
    class_register(CLASS_BOX, granufm_mc_class);

    class_dspinit(c);

    return 0;
}
END_USING_C_LINKAGE
