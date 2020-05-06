#pragma once

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "z_dsp.h"
#include "buffer.h"

#include <vector>
#include "buffer_proxy.hpp"
#include "granu_mc_utils.hpp"
#include <cmath>

struct GranuGrain
{
    bool        m_active = false;
    
    double      m_startpoint; // in samples (floats for interpolation)
    long        m_buf_index;
    
    long        m_wind_index;
    long        m_wind_type = 0;
    
    double      m_shape_x = 1.;
    double      m_shape_y = 1.;
    double      m_wind_norm_coef = 1.0;
    
    std::vector<double> m_chan_amp;

    long        m_buf_chans = 1;
    double      m_buf_len = 0;
    double      m_buf_sr = 0;

    bool        m_loop_mode = false;
    
    long        m_phase_counter = 0;
    long        m_max_count = 0;
    
    long        m_channel_offset = 0;
    long        m_src_channels = 1;
    
    // src channel count
    //  0 = amps used as weights for 1 channel with offset option
    //  >0 = amps used as weights for src channels, if set to 2 works for stereo files,
    
    void set(double start,
             double dur_samps,
             double rate,
             long buffer_index,
             std::vector<double> shape_coef, // if one number then look for window, if two do shaping?, or add another inlet for
             std::vector<double> amps,
             const t_buffer_proxy * buf_proxy,
             const t_buffer_proxy * wind_proxy,
             bool loopmode = false,
             long windowType = 2,
             long channel_offset = 0,
             long src_channels = 1);
    
    
    std::vector<double> incr( t_float *bufferData, long interpType );
    std::vector<double> incr_src_channels( t_float *bufferData, long interpType );

    void reset();
    
    double window(double phase);
    
    
private:
    
    double      m_playlen;
    bool        m_direction;
    //double      m_phase;
    double      m_incr;
    
    inline void setBeta_ab(double a, double b)
    {
        constexpr double minx = (1/DBL_MAX) + 0.00001;
        const double sum = a + b;
        if( sum > 170.0 )
        {
            if( a > b )
            {
                m_shape_x = a - (sum - 170);
                m_shape_y = (b < minx ? minx : b);
            }
            else
            {
                m_shape_x = (a < minx ? minx : a);
                m_shape_y = b - (sum - 170);
            }
        }
        else
        {
            m_shape_x = CLAMP(m_shape_x, minx, 170);
            m_shape_y = CLAMP(m_shape_y, minx, 170);
        }
    }
    
   
};


inline double kumaraswamy(double x, double a, double b)
{ // a and b >= 1
    return a * b * fastPrecisePow(x, a - 1.) * fastPrecisePow( 1. - fastPrecisePow(x, a), b - 1.);
}

inline double kumaraswamy_peak(double a, double b)
{
    if( (a <= 1) && (b <= 1) )
        return 1;
    
    double mode = fastPrecisePow((a - 1)/(a*b - 1), 1. / a);
    return kumaraswamy(mode, a, b);
}


inline double fixDenorm(double x)
{
    return ((fabs(x) < 1e-20f) ? 0.0 : x);

}

// phase input should be 0 < x < 1
// currently I'm not seeing the full range of the curve for a,b < 1, so maybe the phase needs to be scaled somehow
inline double betaNumerator(double x, double a, double b)
{
    const double num = pow(x, a-1) * pow(1-x, b-1);
    
 //   if( std::isinf(num) ) printf("inf! x %f\n", x);
    
    return std::isinf(num) ? 1 : num;
}

inline double clampGammaDouble(double x)
{
    constexpr double minx = (1/DBL_MAX) + 0.00001;
    return CLAMP(x, minx, 170.0);
}

inline double betaFN(double a, double b)
{
    return tgamma(a)*tgamma(b)/tgamma(a+b);
}

inline double lbetaFn(double a, double b)
{
    return exp(lgamma(a)+lgamma(b)-lgamma(a+b));
}


inline double betaPDF(double x, double a, double b)
{
    return pow(x, a-1) * pow(1-x, b-1) / betaFN(a,b); // could cache gammas here
}


inline double betaMode(double a, double b)
{
    if( a > 1 && b > 1)
        return (a-1)/(a+b-2);
    else if( a == 1 && b == 1) // all 1s
        return 0.5;
    else if( a < 1 && b < 1) // bi modal
        return 0.0001;
    else if( a < 1 && b >= 1)
        return 0.0001;
    else if( a >= 1 && b < 1)
        return 0.9999;
    else if( a == 1 && b > 1)
        return 0.0001;
    else if( a > 1 && b == 1)
        return 0.9999;
    else {
        printf("unmatched beta mode %f %f returning 0\n", a, b);
        return 0;
    }
}


inline double getBetaScalar(double a, double b, double stepsize)
{
    
    // reusing denominator: (betaNum / betaDen) == betaPDF
    const double betaDenominator = lbetaFn(a, b);
    
    if( a > 1 && b > 1)
        return 1. / (betaDenominator * ( betaNumerator( (a-1)/(a+b-2), a, b ) / betaDenominator ));
    else if( a > 1 && b == 1 )
        return 1. / (betaDenominator * ( betaNumerator( 1., a, b ) / betaDenominator ));
    else if (a == 1 && b > 1 )
        return 1. / (betaDenominator * ( betaNumerator( 0., a, b ) / betaDenominator ));
    else if (a >= 1 && b < 1 ) // in this case x(1) = inf
        return 1. / (betaDenominator * ( betaNumerator( 1 - stepsize, a, b ) / betaDenominator ));
    else if (a < 1 && b >= 1 ) // in this case x(0) = inf
        return 1. / (betaDenominator * ( betaNumerator( stepsize, a, b ) / betaDenominator ));
    else if (a < 1 && b < 1 )
    {
        if( a > b) // if a > b, then use x(1 - stepsize)
            return 1. / (betaDenominator * ( betaNumerator( 1 - stepsize, a, b ) / betaDenominator ));
        else
            return 1. / (betaDenominator * ( betaNumerator( stepsize, a, b ) / betaDenominator ));
    }
    else if( a == 1 && b == 1 )
        return 1.;
    else
        printf("unknown situation a %f b %f\n", a, b);
    
    return 0;
    
}

inline double betaMax(double a, double b)
{
    return betaPDF( betaMode(a, b), a, b );
}
