// if no window is set, default cosine wave, with x y curve function

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "z_dsp.h"
#include "buffer.h"


#define TEST_NUM_INLETS 2

static t_class *mc_test_class = NULL;

typedef struct _test_mc
{
    t_pxobject  m_obj;
   
    long        numoutputs;
    
    long        totalIns;
    
    long        isConnected[TEST_NUM_INLETS];
    long        inputChannels[TEST_NUM_INLETS];
    long        startsAt[TEST_NUM_INLETS];
    
    
} t_mc_tester;


void mc_tester_perform64(t_mc_tester *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    post("ins %ld outs %ld", numins, numouts );

    for(long k = 0; k < sampleframes; k++)
    {
        for( int i = 0; i < numouts; i++)
        {
            outs[i][k] = ins[i][k];
        }
    }
    
}

void mc_tester_getStartPoints(t_mc_tester *x)
{
    long i, count = 0;
    x->startsAt[0] = 0;
    post("0 starts at 0");
    
    for (i = 1; i < TEST_NUM_INLETS; i++) {
        count += x->inputChannels[i-1];
        x->startsAt[i] = count;
        post("%ld starts at %ld", i, x->startsAt[i]);
    }
    
    x->totalIns = count + x->inputChannels[TEST_NUM_INLETS-1];
    post("totalIns %ld lastInputCount %ld", x->totalIns, x->inputChannels[TEST_NUM_INLETS-1] );
}

void mc_tester_dsp64(t_mc_tester *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    post("dsp64 %p %p", dsp64, dspchain_get() );
    for( long i = 0; i < TEST_NUM_INLETS; i++)
    {
        long n = (long)object_method(dsp64, gensym("getnuminputchannels"), x, i);
        x->isConnected[i] = count[i];
        x->inputChannels[i] = n;
    }
    mc_tester_getStartPoints(x);
    
    x->numoutputs = x->totalIns;
    
    dsp_add64(dsp64, (t_object*)x, (t_perfroutine64)mc_tester_perform64, 0, NULL);
    
}


long mc_tester_multichanneloutputs(t_mc_tester *x, long outletindex)
{
    
    post("getouts %ld", x->numoutputs);
    return x->numoutputs;
    /*
    if( outletindex == 0)
        return x->numoutputs;
    else
        return 1;
     */
}


long mc_tester_inputchanged(t_mc_tester *x, long index, long count)
{
    post("mc_tester_inputchanged %ld %ld", index, count);
    
    if( count != x->inputChannels[index] )
    {
        x->inputChannels[index] = count;
        mc_tester_getStartPoints(x);
        x->numoutputs = x->totalIns;
        return true;
    }

    return false;
    
}


void mc_tester_free(t_mc_tester *x)
{
    dsp_free((t_pxobject *)x);
    
}


void *mc_tester_new(t_symbol *s, long argc, t_atom *argv)
{
    t_mc_tester *x = (t_mc_tester *)object_alloc(mc_test_class);
    
    if (x) {
        
        outlet_new(x, "multichannelsignal");
        x->totalIns = 2;
        x->numoutputs = 2;
        
        dsp_setup((t_pxobject *)x, TEST_NUM_INLETS);
        x->m_obj.z_misc |= Z_MC_INLETS;
        
        
    }
    
    return x;
}


BEGIN_USING_C_LINKAGE

int C74_EXPORT main(void)
{
    t_class *c = class_new("mc.tester~", (method)mc_tester_new, (method)mc_tester_free, (long)sizeof(t_mc_tester), NULL, A_GIMME);
    
    class_addmethod(c, (method)mc_tester_dsp64,  "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)mc_tester_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)mc_tester_inputchanged, "inputchanged", A_CANT, 0);
    
    mc_test_class = c;
    class_register(CLASS_BOX, mc_test_class);
    
    class_dspinit(c);
    
    return 0;
}
END_USING_C_LINKAGE

