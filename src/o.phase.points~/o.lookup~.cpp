

#define NAME "o.phase.points~"
#define DESCRIPTION ""
#define AUTHORS "Rama Gottfried"
#define COPYRIGHT_YEARS "2016-7"

#include "o.phase.points~.hpp"

using namespace std;

static t_class *ophasepoints_class;

typedef struct _ophasepoints {
    t_pxobject  ob;
    
    vector< PhasePoints > phrase;
    
    // chache with phrase update to avoid size() lookup
    t_int       num_phrases;
    
    // sample and hold values if phase doesn't change
    double      cur_phase;
    double      val;
    double      rel_phase;
    t_int       index;
    double      delta_between_points;
    
    int         phrase_index;
    
    bool        update = true;
    
    // attrs
    bool        phaseincr;
    bool        phasewrap;
    bool        normal_x;
    bool        interp;

    
    t_symbol*   base_address;
    size_t      base_address_len;
    size_t      nbytes_selector;
    char **     selectors;
    int         num_selectors;
    char *      selectors_ptrs[8];
    
    void*       osc_outlet;
    long        osc_inlet;
    t_proxy     proxy;
    t_critical  lock;
    
} t_ophasepoints;

void ophhasepoints_FullPacket(t_ophasepoints *x, t_symbol *s, long argc, t_atom *argv)
{
//    post( "%i", proxy_getinlet((t_object *)x) );
    
    OMAX_UTIL_GET_LEN_AND_PTR
    
    // ======================= wrap_naked_osc
    // from wrap_naked... alloca was a problem in C++
    if(ptr && len >= 8){
        if(strncmp("#bundle\0", (char *)(ptr), 8)){
            char *oldptr = (char *)ptr;
            long oldlen = len;
            len += 4 + OSC_HEADER_SIZE;
            ptr = (char *)alloca(len);
            char alloc = 0;
            osc_bundle_s_wrapMessage(oldlen, oldptr, &len, (char **)(&ptr), &alloc);
        }
    }
    
    // maybe don't need to output here...
    if(len == OSC_HEADER_SIZE){
        // omax_util_outletOSC(x->osc_outlet, len, ptr);
        return;
    }
    // ==========================
    
 
    
    
    
    // match all with base address, striping matched portion (i.e. /foo/x, /foo/y, we'll get /x and /y
    // but how should multiple phrases be handled? subbundle? ...
    // previously in patched version, we used /foo/0/x
    // I guess we could also search for /foo/*/x, or a range of /foo/[0-20]/x to find indexed phrases within range of lookup table...
    // if we pre-construct the namespace in the init(), we will have a list of possible addresses, and then will iterate the rset results for *full matches*
    // but, also, think about whether that makes sense...
    // should we accept /x and /dur? or have a mode for that maybe
    
    
    // this is another way to do matching, but not using for now -- see getBundleMember()
    /*
    t_osc_message_array_s *msgar = (len, ptr, selectors[0], 1);
    
    if(msgar){
        t_osc_msg_s *m = osc_message_array_s_get(msgar, 0);

        osc_message_array_s_free(msgar);
    }
    */
    
    
    t_osc_rset *rset = NULL;
    int strip_matched_portion_of_address = 1;
    
    osc_query_select(x->num_selectors,
                     x->selectors_ptrs,
                     (int32_t)len, ptr,
                     strip_matched_portion_of_address,
                     &rset );
    
    if(rset) // all matches
    {
        
        bool found_x = false;
        bool found_y = false;
        bool found_c = false;
        // check if x and y, or x/y/c are found
        // if yes, make lookup table
        
        // maybe don't need to iterate the selectors becuase we will always have the same number...
        // that could maybe make the code easier to read also
        
        found_x = osc_rset_select(rset, x->selectors[0]) != NULL;
        found_y = osc_rset_select(rset, x->selectors[1]) != NULL;
        found_c = osc_rset_select(rset, x->selectors[2]) != NULL;
        
        if( found_x && found_y )
        {
            PhasePoints new_phrase;
        
            for(int i = 0; i < 3; i++) //x->num_selectors
            {
                char *selector = x->selectors[i];
                t_osc_rset_result *res = osc_rset_select(rset, selector);
                
                if(res)
                {
                    t_osc_bndl_s *matches = osc_rset_result_getCompleteMatches( res );
                    
                    // t_osc_bndl_s *partial_matches = osc_rset_result_getPartialMatches(res);
                    
                    t_osc_bndl_it_s *it = osc_bundle_iterator_s_getIterator( osc_bundle_s_getLen(matches),
                                                                             osc_bundle_s_getPtr(matches) );
                    while(osc_bundle_iterator_s_hasNext(it))
                    {
                        t_osc_msg_s *msg = osc_bundle_iterator_s_next(it);
                        char *addr = osc_message_s_getAddress(msg);

                        new_phrase.parseMsg(addr + x->base_address_len, msg, (t_object*)x);
                    }
                    osc_bundle_iterator_s_destroyIterator(it);
                }
            }
            
            if( new_phrase.init( x->normal_x ) )
            {
                critical_enter(x->lock);
                x->phrase[0] = new_phrase;
                x->update = true;
                critical_exit(x->lock);
                
                new_phrase.print();
            }
            
            
        }
        
        osc_rset_free(rset);
    }
    
    post("interp %i norm %i", x->interp, x->normal_x);
    
//    omax_util_outletOSC(x->osc_outlet, len, ptr);
}



void ophasepoints_perform64(t_ophasepoints *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    t_double *phase_in = ins[0];
    t_double *index_in = ins[1];

    t_double *interp_val_out = outs[0];
    t_double *rel_phase_out = outs[1];
    t_double *index_out = outs[2];
    t_double *delta_out = outs[3];

    critical_enter(x->lock);
    vector<PhasePoints> x_phrase = x->phrase;
    long max_phr_idx = x_phrase.size() - 1;
    critical_exit(x->lock);
    
    t_int in_idx, points_len, max_idx0, max_idx1;
    double  in_phase, max_phase, x0 = 0, x1 = 0, y0 = 0, y1 = 0, range = 0, fp = 0;

    double y_val = x->val;
    double phase = x->cur_phase;
    t_int idx = x->index;
    double delta = x->delta_between_points;
    
    double prev_inphase = phase;
    t_int phrase_index = x->phrase_index;
    
    long n = sampleframes;
    
    while (n--)
    {
        if( max_phr_idx == -1 )
        {
            *interp_val_out++ = 0;
            *rel_phase_out++ = 0;
            *index_out++ = 0;
            *delta_out++ = 0;
        }
        else
        {
            in_phase = *phase_in++;
            in_idx = (t_int)*index_in++;
            
            if( in_phase != phase || in_idx != phrase_index || x->update )
            {
                if( x->update )
                {
                    critical_enter(x->lock);
                    x_phrase = x->phrase;
                    max_phr_idx = x_phrase.size() - 1;
                    x->update = false;
                    critical_exit(x->lock);
                }

                phrase_index = (t_int)CLAMP( in_idx, 0, max_phr_idx );
                PhasePoints& phr = x_phrase[phrase_index];
                
                points_len = phr.len;
                max_idx1 = points_len;
                max_idx0 = points_len-1;
                
                // current segment start/end
                x0 = phr.x[ CLAMP(idx, 0, max_idx0) ];
                x1 = phr.x[ CLAMP(idx+1, 1, max_idx1) ];
                
                if( x->phaseincr == 1 )
                {
                    in_phase = prev_inphase + in_phase;
                }
                
                if( x->phasewrap == 1 )
                {
                    max_phase = x->normal_x == 1 ? 1 : max_idx0;
                    in_phase = fmod(in_phase, max_phase);
                }
                
                prev_inphase = in_phase;
                
                if( in_phase < x0 ){
                    while( in_phase < x0 && idx-- > 0 ){
                        x0 = phr.x[ CLAMP(idx, 0, max_idx0) ];
                    }
                    
                    if( in_phase < x0 && idx <= 0 )
                        x1 = x0;
                    else
                        x1 = phr.x[  CLAMP(idx+1, 1, max_idx1) ];
                    
                } else if( in_phase >= x1 ) {
                    while( in_phase >= x1 && idx++ < points_len ){
                        x1 = phr.x[ CLAMP(idx+1, 1, max_idx1) ];
                    }
                    
                    if( in_phase > x1 && idx >= points_len )
                        x0 = x1;
                    else
                        x0 = phr.x[  CLAMP(idx, 0, max_idx0) ];
                }
                
                delta = x1 - x0;

                if( idx >= max_idx1 && x0 == x1 )
                {
                    phase = 1;
                }
                else if( delta > 0 )
                {
                    phase = (in_phase - x0) / delta;
                }
                else
                    phase = 0;
                
                idx = CLAMP(idx, 0, max_idx0);
                
                
                if( !x->interp )
                {
                    y_val = phr.y[ CLAMP(idx, 0, max_idx0) ];
                }
                else
                {
                    // get y positions and interpolate a la curve~
                    y0 = phr.y[ CLAMP(idx, 0, max_idx0) ];
                    y1 = phr.y[ CLAMP(idx+1, 1, max_idx1) ];
                    range = y1-y0;
                    
                    // fp = peek(cbuf, clip(idx+1, 1, max_idx1), channel, channels=1);
                    fp = 0;
                    if( fp == 0 )
                        y_val = y0 + phase*range;
                    else {
                        //gp = (exp(fp * phase) - 1.) / (exp(fp) - 1.) ;
                        //y_val = y0 + gp*range;
                        y_val = -1; //temp
                    }
                }
                
            }
            
            *interp_val_out++ = y_val;
            *rel_phase_out++ = phase;
            *index_out++ = idx;
            *delta_out++ = delta;
        }
        
    }

    x->val = y_val;
    x->cur_phase = phase;
    x->index = idx;
}

void ophasepoints_dsp64(t_ophasepoints *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    object_method(dsp64, gensym("dsp_add64"), x, ophasepoints_perform64, 0, NULL);
}

void ophasepoints_assist(t_ophasepoints *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) { //inlet
        switch (a) {
            case 0:
                sprintf(s, "(signal) phase x axis lookup");
                break;
            case 1:
                sprintf(s, "(signal) phrase index lookup");
                break;
            case 2:
                sprintf(s, "(FullPacket) OSC input");
                break;
        }
    }
    else
    {	// outlet
        switch (a) {
            case 0:
                sprintf(s, "(signal) phrase value at lookup point");
                break;
            case 1:
                sprintf(s, "(signal) relative phase");
                break;
            default:
                sprintf(s, "outlet %ld", a);
                break;
        }
    }
    
}

void ophasepoints_free(t_ophasepoints *x)
{
    
    dsp_free(&(x->ob));

    critical_free( x->lock );
    
    object_free(x->proxy);
    
    for( int i = 0; i < 8; i++)
    {
        free(x->selectors[i]);
    }
    
    free(x->selectors);
    
}

void *ophasepoints_new(t_symbol* s, short argc, t_atom* argv)
{
    t_ophasepoints *x = (t_ophasepoints *)object_alloc(ophasepoints_class);
    if(x)
    {
        
        if( !argc || atom_gettype(argv) != A_SYM ) // missing base address
        {
            object_error((t_object*)x, "missing base OSC address");
            return NULL;
        }
        
        // maybe we don't need a base address for this object...?
        
        x->base_address = atom_getsym(argv);
        char *base_addr = x->base_address->s_name;
        x->base_address_len = strlen(base_addr);
        x->nbytes_selector = x->base_address_len + 8; // longest size
        
        x->num_selectors = 8;
        x->selectors = (char **)malloc(8 * sizeof(char *));
        for( int i = 0; i < 8; i++)
        {
            x->selectors[i] = (char *)malloc(x->nbytes_selector * sizeof(char));
            x->selectors_ptrs[i] = x->selectors[i];
        }
        
        x->interp = 1;
        x->phaseincr = 0;
        x->phasewrap = 0;
        x->normal_x = 1;
        x->interp = 1;
        
        // set these as selector addresses
        char buf[x->nbytes_selector];
        sprintf(buf, "%s/x", base_addr);
        strcpy( x->selectors[0], buf );
        
        sprintf(buf, "%s/y", base_addr);
        strcpy( x->selectors[1], buf );
        
        sprintf(buf, "%s/curve", base_addr);
        strcpy( x->selectors[2], buf );
        
        sprintf(buf, "%s/dur", base_addr);
        strcpy( x->selectors[3], buf );
        
        sprintf(buf, "%s/*/x", base_addr);
        strcpy( x->selectors[4], buf );
        
        sprintf(buf, "%s/*/y", base_addr);
        strcpy( x->selectors[5], buf );
        
        sprintf(buf, "%s/*/curve", base_addr);
        strcpy( x->selectors[6], buf );
        
        sprintf(buf, "%s/*/dur", base_addr);
        strcpy( x->selectors[7], buf );
        
        dsp_setup((t_pxobject *)x, 2);
        
        x->osc_outlet = outlet_new((t_object *)x, "FullPacket");
        outlet_new((t_object *)x, "signal");
        outlet_new((t_object *)x, "signal");
        outlet_new((t_object *)x, "signal");
        outlet_new((t_object *)x, "signal");
        
        x->proxy = proxy_new((t_object *)x, 1, &(x->osc_inlet));
        
        critical_new( &x->lock );

    }
    return (void *)x;
}

BEGIN_USING_C_LINKAGE
int C74_EXPORT main(void)
{
    
    t_class *c = class_new("o.phase.points~", (method)ophasepoints_new, (method)ophasepoints_free, sizeof(t_ophasepoints), 0L, A_GIMME, 0);
    
    class_addmethod(c, (method)ophasepoints_dsp64,          "dsp64",        A_CANT,     0);
    class_addmethod(c, (method)ophasepoints_assist,         "assist",       A_CANT,     0);
    class_addmethod(c, (method)ophhasepoints_FullPacket,	"FullPacket",	A_GIMME,    0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    ophasepoints_class = c;
    
    post("%s by %s.", NAME, AUTHORS);
    post("Copyright (c) " COPYRIGHT_YEARS " Regents of the University of California.  All rights reserved.");
}
END_USING_C_LINKAGE
