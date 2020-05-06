
#include "grain.hpp"

/**
 *  what's better, phase counter vs phase incr?
 *  doing phase += incr is nice for simplicy, but to really hit the last step is difficult due to rounding errors
 *
 *  src_channels:
    if no value set then the source channels are based on the amplist input
    if no value set and no amplist attached, what is default?
 *          do we need to iterated the polybuffer and find the largest number of channels?
 *          seemes better to make the user specify this, since mc. is a fixed number of output channels
 *
 *  ok so: default src_channels 1, default amps 1 channel unity
 *      if src_channels > 1, default amps is 1 for each channel
 */

void GranuGrain::reset()
{
    m_active = false;
    m_chan_amp = {1};
    m_buf_len = 0;
    m_incr = 0;
    m_shape_x = 1.;
    m_shape_y = 1.;
    m_phase_counter = 0;
}

void GranuGrain::set(double start,
         double dur_samps,
         double rate,
         long buffer_index,
         std::vector<double> shape_coef,
         std::vector<double> amps,
         const t_buffer_proxy * buf_proxy,
         const t_buffer_proxy * wind_proxy,
         bool loopmode,
         long windowType,
         long channel_offset,
         long src_channels )
{
    m_buf_len = buf_proxy->nframes - 1;
    m_buf_chans = buf_proxy->nchans;
    m_buf_sr = buf_proxy->sr;
    m_buf_index = buffer_index;
    m_src_channels = src_channels;
    
   // m_src_channels = CLAMP(src_channels, 1, m_buf_chans);
    m_channel_offset = CLAMP(channel_offset, 0, m_buf_chans - 1);
    
  //  printf("incr_src_channels %ld buf chans %ld\n", m_src_channels, m_buf_chans);

  //  post("shape coef size %ld %f", shape_coef.size(), shape_coef[0] );
    
    m_startpoint = CLAMP(start, 0, 1) * m_buf_len;

    // duration of grain
    // or if negative use the length of the original sample scaled by ratio
    
    double gr_dur = (dur_samps <= 0) ? m_buf_len : dur_samps;
    
    // phase goes 0-1
    // if rate is negative, then play backwards
    // to do this some decision has to be made,
    // for instance, right now we will use the same "start" and "end" points,
    // but use a negative incr and start the phase at 1 if backwards
    m_direction = rate < 0;

    rate = abs(rate) * buf_proxy->rate_scalar;
    
    m_loop_mode = loopmode;
    if( loopmode )
    {
        double playlen = round(rate * gr_dur);
        m_playlen = playlen;
    }
    else
    {
        gr_dur = (gr_dur >= m_buf_len) ? m_buf_len : gr_dur;
        double playlen = round(rate * gr_dur);
        double playmax = m_buf_len - m_startpoint;
        m_playlen = ( playlen <= playmax ) ? playlen : playmax;
    }
    
    m_incr = rate / m_playlen; // or m_playlen - 1?
    m_max_count = round(m_playlen / rate);
    

    //m_phase = rate < 0;
    //printf("direction %i playlen %f count max %ld\n", m_direction, m_playlen, m_max_count);

    /*
     poke(m_panL,     fastcos( pan * quarterPI),    i, 0, 0);
     poke(m_panR,     fastsin( pan * quarterPI),    i, 0, 0);
     poke(m_xexp,     xexp,                             i, 0, 0);
     poke(m_yexp,     yexp,                             i, 0, 0);
     _count += 1;
     */
    
    if( m_src_channels > 1 )
    {
        if( amps.size() == m_src_channels )
            m_chan_amp = amps;
        else
        {
            m_chan_amp.clear();
            m_chan_amp.reserve(m_src_channels);
            
            for( int i = 0; i < m_src_channels; i++ )
            {
                m_chan_amp[i] = amps[0];
            }
        }
    }
    else
    {
        m_chan_amp = amps;
    }
    
    m_wind_type = windowType;

    // to do soon: deal with how to switch window types, buffers?
    if(shape_coef.size() == 2)
    {
        m_shape_x = shape_coef[0];
        m_shape_y = shape_coef[1];
    }
    
    if( m_wind_type == 0 )
    {
        
    //    printf("coef %f %f \n", m_shape_x, m_shape_y);
        
        setBeta_ab(m_shape_x, m_shape_y);
        m_wind_norm_coef = getBetaScalar(m_shape_x, m_shape_y, m_incr);
       // printf("coef %f\n", m_wind_norm_coef);
    }
    else if( m_wind_type == 2 )
    {
        m_shape_x = m_shape_x < 1 ? 2. : m_shape_x;
        m_shape_y = m_shape_y < 1 ? 2. : m_shape_y;
        m_wind_norm_coef = 1. / kumaraswamy_peak(m_shape_x, m_shape_y);
    }
    else
        m_wind_norm_coef = 1;
        
        // type 1 cos has no precalc coeffs
        
        //post("%f", m_wind_max);
    
    m_active = true;

    
    
}
/*
double betaMode(double a, double b)
{
        t_pt mode;
        if(a > 1 && b > 1){
            mode.x = (a - 1) / (a + b - 2);
            mode.y = gsl_ran_beta_pdf(mode.x, a, b);
        }else if(a == 1 && b == 1){
            mode.x = .5;
            mode.y = 1.;
        }else{
            double left, right;
            left = gsl_ran_beta_pdf(.0001, a, b);
            right = gsl_ran_beta_pdf(.9999, a, b);
            if(right >= left){
                mode.x = .0001;
                mode.y = right;
            }else{
                mode.x = .9999;
                mode.y = left;
            }
        }
}
*/


double GranuGrain::window(double phase)
{
    switch (m_wind_type) {
        case 0:
            /*
             {
             double v = betaNumerator(phase, m_shape_x, m_shape_y) * m_wind_norm_coef;
             if( v > 1 || v < 0 )
             printf("out of bounds %.17g (phase %.17g, 1 - incr %.17g incr %.17g counter %ld max %ld)\n", v, phase, 1-m_incr, m_incr, m_phase_counter, m_max_count );
             
             }
             */
            return CLAMP(betaNumerator(phase, m_shape_x, m_shape_y) * m_wind_norm_coef, 0., 1.);
        case 1:
        {
            double px = pow(phase, exp(m_shape_x)); //fastPrecisePow
            double ax = sin( PI * px);
            return pow( ax, exp(m_shape_y));
        }
        case 2:
            return kumaraswamy(phase, m_shape_x, m_shape_y) * m_wind_norm_coef;
        default:
            return 0;
    }
}

typedef enum _granu_interp
{
    NONE,
    LINEAR,
    CUBIC
} eGInterp;

std::vector<double> GranuGrain::incr( t_float *bufferData, long interpType )
{
    size_t nchans = m_chan_amp.size();
    
    std::vector<double> amps;
    amps.reserve(nchans);
    // allocation, should probably avoid this, try to figure it out when the dsp resets, and init all the grain voices
    
    double _playSamp = 0;
    double _sampIdx = 0;
    
    double phase = m_phase_counter == m_max_count ? 1. : m_phase_counter * m_incr;
    
    if( m_direction ) // true == backwards
        _sampIdx = m_startpoint + ((1 - phase) * m_playlen);
    else
        _sampIdx = m_startpoint + (phase * m_playlen);

    
    if( m_loop_mode )
        _sampIdx = wrapDouble(_sampIdx, m_buf_len);
    
    
    
    double lowerSamp, upperSamp, frac, a1, b, c, d, upperVal;
    
    switch (interpType)
    {
        case LINEAR:
            lowerSamp = floor(_sampIdx);
            upperSamp = ceil(_sampIdx);
            if( m_loop_mode )
                upperVal = bufferData[ m_buf_chans * (long)wrapDouble(upperSamp, m_buf_len) + m_channel_offset ];
            else
                upperVal = (upperSamp < m_buf_len) ? bufferData[ m_buf_chans * (long)upperSamp + m_channel_offset ] : 0.0;
            
            _playSamp = linear_interp( bufferData[ m_buf_chans * (long)lowerSamp + m_channel_offset ], upperVal, _sampIdx - lowerSamp);
            break;
            
        case CUBIC:
            lowerSamp = floor(_sampIdx);
            frac = _sampIdx - lowerSamp;
            
            if( m_loop_mode ){
                a1 = (long)lowerSamp - 1 < 0 ? 0 : bufferData[ m_buf_chans * (long)wrapDouble(lowerSamp - 1, m_buf_len) + m_channel_offset ];
                b = bufferData[ m_buf_chans * (long)lowerSamp + m_channel_offset ];
                c = bufferData[ m_buf_chans * (long)wrapDouble(lowerSamp + 1, m_buf_len) + m_channel_offset ];
                d = bufferData[ m_buf_chans * (long)wrapDouble(lowerSamp + 2, m_buf_len) + m_channel_offset ];
            }
            else
            {
                a1 = (long)lowerSamp - 1 < 0 ? 0 : bufferData[ m_buf_chans * ((long)lowerSamp - 1) + m_channel_offset ];
                b = bufferData[ m_buf_chans * (long)lowerSamp + m_channel_offset ];
                c = (long)lowerSamp + 1 >= m_buf_len ? 0 : bufferData[ m_buf_chans * ((long)lowerSamp + 1) + m_channel_offset ];
                d = (long)lowerSamp + 2 >= m_buf_len ? 0 : bufferData[ m_buf_chans * ((long)lowerSamp + 2) + m_channel_offset ];
            }
           
            
            _playSamp = cubicInterpolate(a1,b,c,d,frac);
            break;
            
        case NONE:
        default:
            _playSamp = bufferData[ m_buf_chans * (long)_sampIdx + m_channel_offset ];
            break;
    }
/*
    // window
    double px = fastPrecisePow(m_phase, exp(m_shape_x));
    double ax = sin( PI * px);
    double windX = fastPrecisePow( ax, exp(m_shape_y));
*/
    
   
    _playSamp *= window(phase);

    for( long i = 0 ; i < nchans; i++ )
    {
        amps.emplace_back( _playSamp * m_chan_amp[i] );
        // to do: pre-allocate everything when dsp is reset and don't use dynamic memory here
    }
    
    //m_phase += m_incr;
    m_phase_counter++;
    
    if(  m_phase_counter > m_max_count ) {
     //   printf("released at phase %.17g prev %.17g incr %.17g\n", phase, phase-m_incr, m_incr );
     //   printf("counter %ld maxcount %ld \n", m_phase_counter, m_max_count );
        reset();
    }
    return amps;
}



std::vector<double> GranuGrain::incr_src_channels( t_float *bufferData, long interpType )
{
    // output amps is the src_channels here
    size_t nchans = m_src_channels;
    
    std::vector<double> amps;
    amps.reserve(nchans);
    // to do: pre-allocate everything when dsp is reset and don't use dynamic memory here

   // printf("incr_src_channels %ld\n", nchans);
    
    double _sampIdx = 0;
    
    double phase = m_phase_counter == m_max_count ? 1. : m_phase_counter * m_incr;
    
    if( m_direction ) // true == backwards
        _sampIdx = m_startpoint + ((1 - phase) * m_playlen);
    else
        _sampIdx = m_startpoint + (phase * m_playlen);

    
    if( m_loop_mode )
        _sampIdx = wrapDouble(_sampIdx, m_buf_len);
    
    
    
    double lowerSamp, upperSamp, frac, a1, b, c, d, upperVal;
    double _playSamp = 0;

    for( int i = 0; (i < nchans) && (i < m_buf_chans); i++)
    {
        switch (interpType)
            {
                case LINEAR:
                    lowerSamp = floor(_sampIdx);
                    upperSamp = ceil(_sampIdx);
                    
                    
                    if( m_loop_mode )
                        upperVal = bufferData[ i + m_buf_chans * (long)wrapDouble(upperSamp, m_buf_len) ];
                    else
                        upperVal = (upperSamp < m_buf_len) ? bufferData[ i + m_buf_chans * (long)upperSamp ] : 0.0;
                    
                    _playSamp = linear_interp( bufferData[ i + m_buf_chans * (long)lowerSamp + m_channel_offset ], upperVal, _sampIdx - lowerSamp);
                    break;
                    
                case CUBIC:
                    lowerSamp = floor(_sampIdx);
                    frac = _sampIdx - lowerSamp;
                    
                    if( m_loop_mode ){
                        a1 = (long)lowerSamp - 1 < 0 ? 0 : bufferData[ i + m_buf_chans * (long)wrapDouble(lowerSamp - 1, m_buf_len)  ];
                        b = bufferData[ i + m_buf_chans * (long)lowerSamp  ];
                        c = bufferData[ i + m_buf_chans * (long)wrapDouble(lowerSamp + 1, m_buf_len) ];
                        d = bufferData[ i + m_buf_chans * (long)wrapDouble(lowerSamp + 2, m_buf_len) ];
                    }
                    else
                    {
                        a1 = (long)lowerSamp - 1 < 0 ? 0 : bufferData[ i + m_buf_chans * ((long)lowerSamp - 1)  ];
                        b = bufferData[ i + m_buf_chans * (long)lowerSamp  ];
                        c = ((long)lowerSamp + 1) >= m_buf_len ? 0 : bufferData[ i + m_buf_chans * ((long)lowerSamp + 1) ];
                        d = ((long)lowerSamp + 2) >= m_buf_len ? 0 : bufferData[ i + m_buf_chans * ((long)lowerSamp + 2) ];
                    }
                   
                    
                    _playSamp = cubicInterpolate(a1,b,c,d,frac);
                    break;
                    
                case NONE:
                default:
                    _playSamp = bufferData[ i + m_buf_chans * (long)_sampIdx  ];
                    break;
            }
        /*
            // window
            double px = fastPrecisePow(m_phase, exp(m_shape_x));
            double ax = sin( PI * px);
            double windX = fastPrecisePow( ax, exp(m_shape_y));
        */
            
           
            _playSamp *= window(phase);
     
            amps.emplace_back( _playSamp * m_chan_amp[i] );
        // to do: pre-allocate everything when dsp is reset and don't use dynamic memory here

    }

    long missingCh = nchans - m_buf_chans;
    if( missingCh > 0)
    {
        while( missingCh-- )
        {
            amps.emplace_back( amps.back() );
        }
    }
    
    //m_phase += m_incr;
    m_phase_counter++;
    
    if(  m_phase_counter > m_max_count ) {
     //   printf("released at phase %.17g prev %.17g incr %.17g\n", phase, phase-m_incr, m_incr );
     //   printf("counter %ld maxcount %ld \n", m_phase_counter, m_max_count );
        reset();
    }
    return amps;
}

