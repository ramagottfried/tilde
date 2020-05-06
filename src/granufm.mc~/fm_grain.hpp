#pragma once

#include "ext.h"
#include "ext_obex.h"
#include "ext_common.h"
#include "z_dsp.h"
#include "buffer.h"

#include <vector>

#include "granu_mc_utils.hpp"
#include <cmath>

struct GranuFMGrain
{
  
    bool        m_active = false;

     //double index,
    void set(double dur_samps,
             double car_incr,
             double harm,
             std::vector<double> shape_coef, // if one number then look for window, if two do shaping?, or add another inlet for
             std::vector<double> amps,
             long windowType);
    
    
    std::vector<double> incr(double _mod_index);

    std::vector<double> incr(double _mod_index, double * waveTable, long tabsize );

    
    void reset();
    
    double window(double phase);
    
    
private:
    
    long        m_phase_counter = 0;
    long        m_max_count = 0;
    
    double      m_car, m_harm, m_index; // index not used for now (keeping it realtime)
    double      m_car_incr;
    double      m_wind_incr;
    
    
    std::vector<double> m_chan_amp;

    long        m_wind_type = 0;
    
    double      m_shape_x = 2.0;
    double      m_shape_y = 2.0;
    double      m_wind_norm_coef = 1.0;
   
    
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


inline double FM(double *sinetab, long tablesize, double car_pc, double harm, double index  )
{
    double mod_pc = sinetab[(uint32_t)wrapDouble(car_pc * harm, tablesize)] * tablesize;
    return sinetab[ (uint32_t)(wrapDouble(car_pc + (index * mod_pc), tablesize) )];
}


inline double FM(double _phase, double _harm, double _index )
{
    double _mod_pc = cos(TWOPI * _phase * _harm);
    return cos( (TWOPI * _phase) + (_index * _harm * _mod_pc) );
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
