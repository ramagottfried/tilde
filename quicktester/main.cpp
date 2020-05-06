/*
#include <iostream>


double wrapDouble(double p_current, double tablesize)
{
    if( p_current < 0)
        return fmod(p_current, tablesize) + tablesize;
    else return fmod(p_current, tablesize);
}

int32_t main(int32_t argc, const char * argv[]) {
    
    double phase = -1.2;
    
    
    
    std::cout << wrapDouble(phase, 1.) << "\n";
    return 0;
}
*/

#include <iostream>
#include <cmath>
#include <ctime>

// uses union, no -fno-alias... required
inline double fastPow(const double a, const double b) {
    union {
        double d;
        int32_t x[2];
    } u = { a };
    u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;
    return u.d;
}

inline double fastStructPow(const double a, const double b) {
    union {
        double d;
        struct {
            int32_t a;
            int32_t b;
        } s;
    } u = { a };
    u.s.b = (int)(b * (u.s.b - 1072632447) + 1072632447);
    u.s.a = 0;
    return u.d;
}


// old version
inline double oldFastPow(const double a, const double b)
{
    int32_t tmp = *(1 + (int*)&a);
    int32_t tmp2 = (int)(b * (tmp - 1072632447) + 1072632447);
    double p = 0.0;
    *(1 + (int32_t * )&p) = tmp2;
    return p;
}

// should be much more precise with large b
inline double fastPrecisePow(double a, const double b) {
    // calculate approximation with just the fraction of the exponent
    int32_t exp = (int32_t) b;
    union {
        double d;
        int32_t x[2];
    } u = { a };
    u.x[1] = (int32_t)((b - exp) * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;
    
    // exponentiation by squaring with the exponent's integer part
    // double r = u.d makes everything much slower, not sure why
    double r = 1.0;
    while (exp) {
        if (exp & 1) {
            r *= a;
        }
        a *= a;
        exp >>= 1;
    }
    
    return r * u.d;
}


void fastpowtest()
{
    
    double x_max = 4.5423;
    double y_max = 32.3211;
    
    size_t times = 100;
    double dx = x_max / times;
    double dy = y_max / times;
    
    // warmup
    
    std::clock_t start;
    double duration;
    
    start = std::clock();
    
    double sum = 0;
    double x = 0.1;
    double y = 0.1;
    double a = 1.0 / times;
    for (size_t i=0; i<times; ++i) {
        x += dx;
        y += dy;
        sum += x * y + x + y / (i - y);
    }
    duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout<<"printf: "<< duration <<'\n';
    
    std::cout << sum << " warmup" << std::endl << std::endl;
    
    
    // original pow ///////////////////////////////////
    start = std::clock();
    sum = 0;
    x = 0;
    y = 0;
    for (size_t i=0; i<times; ++i) {
        x += dx;
        y += dy;
        sum += pow(x, y);
    }
    double t_pow = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout << sum << " pow() " << t_pow << " sec" << std::endl;
    
    
    // fastPow ////////////////////////////////
    start = std::clock();
    sum = 0;
    x = 0;
    y = 0;
    for (size_t i=0; i<times; ++i) {
        x += dx;
        y += dy;
        sum += fastPow(x, y);
    }
    double t_fastPow = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout << sum << " fastPow() " << t_fastPow << " sec, speedup=" << (t_pow / t_fastPow) << std::endl;
    
    // oldFastPow ////////////////////////////////
    start = std::clock();
    sum = 0;
    x = 0;
    y = 0;
    for (size_t i=0; i<times; ++i) {
        x += dx;
        y += dy;
        sum += oldFastPow(x, y);
    }
    double t_oldFastPow = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout << sum << " oldFastPow() " << t_oldFastPow << " sec, speedup=" << (t_pow / t_oldFastPow) << std::endl;
    
    // fastStructPow ////////////////////////////////
    start = std::clock();
    sum = 0;
    x = 0;
    y = 0;
    for (size_t i=0; i<times; ++i) {
        x += dx;
        y += dy;
        sum += fastStructPow(x, y);
    }
    double t_fastStructPow = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout << sum << " fastStructPow() " << t_fastStructPow << " sec, speedup=" << (t_pow / t_fastStructPow) << std::endl;
    
    
    // fastPrecisePow ////////////////////////////////
    start = std::clock();
    sum = 0;
    x = 0;
    y = 0;
    for (size_t i=0; i<times; ++i) {
        x += dx;
        y += dy;
        sum += fastPrecisePow(x, y);
    }
    double t_fastPrecisePow = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout << sum << " fastPrecisePow() " << t_fastPrecisePow << " sec, speedup=" << (t_pow / t_fastPrecisePow) << std::endl;
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
        return 0.000001;
    else if( a < 1 && b >= 1)
        return 0.000001;
    else if( a >= 1 && b < 1)
        return 0.999999;
    else if( a == 1 && b > 1)
        return 0.000001;
    else if( a > 1 && b == 1)
        return 0.999999;
    else {
        printf("unmatched beta mode %f %f returning 0\n", a, b);
        return 0;
    }
}

inline double betaNumerator(double x, double a, double b)
{
    return pow(x, a-1) * pow(1-x, b-1);
}


inline double getBetaScalar(double a, double b, double stepsize)
{
    
    // reusing denominator: (betaNum / betaDen) == betaPDF
    const double betaDenominator = betaFN(a, b);
    
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
    else if (a < 1 && b < 1 ){
        if( a > b) // if a > b, then use x(1 - stepsize)
            return 1. / (betaDenominator * ( betaNumerator( 1 - stepsize, a, b ) / betaDenominator ));
        else
            return 1. / (betaDenominator * ( betaNumerator( stepsize, a, b ) / betaDenominator ));
    }
    else
        printf("unknown situation a %f b %f\n", a, b);
    
    return 0;
    
}




int32_t main() {
    
    long len = 100;
    double incr = 1. / ((double)len-1);
    
    double a = 1;
    double b = 0.6;
    
    
    
    for( long i = 0; i < len; i++ )
    {
        std::cout << betaPDF( (double)i*incr, a, b) << " ";
    }
    
    std::cout << "\nmode " << betaPDF( incr * (betaMode(a, b) + 1), a, b)  <<  std::endl;
  
    
    a = 0.9999;
    b = 0.6;
    for( long i = 0; i < len; i++ )
    {
        std::cout << betaPDF( (double)i*incr, a, b) << " ";
    }
    std::cout << "\nmode " << betaPDF(incr * (betaMode(a, b) + 1), a, b)  <<  std::endl;

    
    return 0;
    
    
    
}
