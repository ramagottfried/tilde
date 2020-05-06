
#include "fm_grain.hpp"

/**
 *  what's better, phase counter vs phase incr?
 *  doing phase += incr is nice for simplicy, but to really hit the last step is difficult due to rounding errors
 */

void GranuFMGrain::reset()
{
    m_active = false;
    m_chan_amp = {0};
    m_car_incr = 0;
    m_shape_x = 2.;
    m_shape_y = 2.;
    m_phase_counter = 0;
}

void GranuFMGrain::set(double dur_samps,
                       double car_incr,
                       double harm,
                       std::vector<double> shape_coef, // if one number then look for window, if two do shaping?, or add another inlet for
                       std::vector<double> amps,
                       long windowType )
    {
   
    m_car_incr = car_incr;
    m_harm = harm;
        
    m_max_count = dur_samps - 1;

    m_wind_incr = 1. / m_max_count;
        
    m_chan_amp = amps;
    
    m_wind_type = windowType;

    if(shape_coef.size() == 2)
    {
        m_shape_x = shape_coef[0];
        m_shape_y = shape_coef[1];
        
        if( m_wind_type == 0 )
        {
            setBeta_ab(m_shape_x, m_shape_y);
            m_wind_norm_coef = getBetaScalar(m_shape_x, m_shape_y, m_wind_incr );
           // printf("coef %f\n", m_wind_norm_coef);
        }
        else
        {
            m_wind_norm_coef = 1;
        }
        
    }
    
    m_active = true;

    
    
}


double GranuFMGrain::window(double phase)
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
            double px = fastPrecisePow(phase, exp(m_shape_x));
            double ax = sin( PI * px);
            return fastPrecisePow( ax, exp(m_shape_y));
        }
        default:
            return 0;
    }
}

std::vector<double> GranuFMGrain::incr(double _mod_index)
{
    size_t nchans = m_chan_amp.size();
    
    std::vector<double> amps;
    amps.reserve(nchans);
    
    double _playSamp = 0;

    double car_phase;
    double wind_phase;
    
    if( m_phase_counter == m_max_count )
    {
        car_phase = 1;
        wind_phase = 1;
    }
    else
    {
        car_phase = m_phase_counter * m_car_incr;
        wind_phase = m_phase_counter * m_wind_incr;
    }
    
    _playSamp = FM(car_phase, m_harm, _mod_index) * window(wind_phase);

    for( long i = 0 ; i < nchans; i++ )
    {
        amps.emplace_back( _playSamp * m_chan_amp[i] );
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


std::vector<double> GranuFMGrain::incr(double _mod_index, double * waveTable, long tabsize )
{
    size_t nchans = m_chan_amp.size();
    
    std::vector<double> amps;
    amps.reserve(nchans);
    
    double _playSamp = 0;
    
    double car_phase;
    double wind_phase;
    
    if( m_phase_counter == m_max_count )
    {
        car_phase = 1;
        wind_phase = 1;
    }
    else
    {
        car_phase = m_phase_counter * m_car_incr;
        wind_phase = m_phase_counter * m_wind_incr;
    }
    
    _playSamp = FM(waveTable, tabsize, car_phase, m_harm, _mod_index ) * window(wind_phase);
    
    for( long i = 0 ; i < nchans; i++ )
    {
        amps.emplace_back( _playSamp * m_chan_amp[i] );
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


