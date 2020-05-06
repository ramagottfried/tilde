
#include "granu_mc_utils.hpp"


//credits: http://www.paulinternet.nl/?page=bicubic
double bicubicInterp_acc(double p[4][4], double x, double y)
{
    double a00, a01, a02, a03;
    double a10, a11, a12, a13;
    double a20, a21, a22, a23;
    double a30, a31, a32, a33;
    
    a00 = p[1][1];
    a01 = -.5*p[1][0] + .5*p[1][2];
    a02 = p[1][0] - 2.5*p[1][1] + 2*p[1][2] - .5*p[1][3];
    a03 = -.5*p[1][0] + 1.5*p[1][1] - 1.5*p[1][2] + .5*p[1][3];
    a10 = -.5*p[0][1] + .5*p[2][1];
    a11 = .25*p[0][0] - .25*p[0][2] - .25*p[2][0] + .25*p[2][2];
    a12 = -.5*p[0][0] + 1.25*p[0][1] - p[0][2] + .25*p[0][3] + .5*p[2][0] - 1.25*p[2][1] + p[2][2] - .25*p[2][3];
    a13 = .25*p[0][0] - .75*p[0][1] + .75*p[0][2] - .25*p[0][3] - .25*p[2][0] + .75*p[2][1] - .75*p[2][2] + .25*p[2][3];
    a20 = p[0][1] - 2.5*p[1][1] + 2*p[2][1] - .5*p[3][1];
    a21 = -.5*p[0][0] + .5*p[0][2] + 1.25*p[1][0] - 1.25*p[1][2] - p[2][0] + p[2][2] + .25*p[3][0] - .25*p[3][2];
    a22 = p[0][0] - 2.5*p[0][1] + 2*p[0][2] - .5*p[0][3] - 2.5*p[1][0] + 6.25*p[1][1] - 5*p[1][2] + 1.25*p[1][3] + 2*p[2][0] - 5*p[2][1] + 4*p[2][2] - p[2][3] - .5*p[3][0] + 1.25*p[3][1] - p[3][2] + .25*p[3][3];
    a23 = -.5*p[0][0] + 1.5*p[0][1] - 1.5*p[0][2] + .5*p[0][3] + 1.25*p[1][0] - 3.75*p[1][1] + 3.75*p[1][2] - 1.25*p[1][3] - p[2][0] + 3*p[2][1] - 3*p[2][2] + p[2][3] + .25*p[3][0] - .75*p[3][1] + .75*p[3][2] - .25*p[3][3];
    a30 = -.5*p[0][1] + 1.5*p[1][1] - 1.5*p[2][1] + .5*p[3][1];
    a31 = .25*p[0][0] - .25*p[0][2] - .75*p[1][0] + .75*p[1][2] + .75*p[2][0] - .75*p[2][2] - .25*p[3][0] + .25*p[3][2];
    a32 = -.5*p[0][0] + 1.25*p[0][1] - p[0][2] + .25*p[0][3] + 1.5*p[1][0] - 3.75*p[1][1] + 3*p[1][2] - .75*p[1][3] - 1.5*p[2][0] + 3.75*p[2][1] - 3*p[2][2] + .75*p[2][3] + .5*p[3][0] - 1.25*p[3][1] + p[3][2] - .25*p[3][3];
    a33 = .25*p[0][0] - .75*p[0][1] + .75*p[0][2] - .25*p[0][3] - .75*p[1][0] + 2.25*p[1][1] - 2.25*p[1][2] + .75*p[1][3] + .75*p[2][0] - 2.25*p[2][1] + 2.25*p[2][2] - .75*p[2][3] - .25*p[3][0] + .75*p[3][1] - .75*p[3][2] + .25*p[3][3];
    
    
    double x2 = x * x;
    double x3 = x2 * x;
    double y2 = y * y;
    double y3 = y2 * y;
    
    return (a00 + a01 * y + a02 * y2 + a03 * y3) +
    (a10 + a11 * y + a12 * y2 + a13 * y3) * x +
    (a20 + a21 * y + a22 * y2 + a23 * y3) * x2 +
    (a30 + a31 * y + a32 * y2 + a33 * y3) * x3;
    
}


float cos_52s(float x)
{
    const float c1= 0.9999932946f;
    const float c2=-0.4999124376f;
    const float c3= 0.0414877472f;
    const float c4=-0.0012712095f;
    float x2;      // The input argument squared
    x2=x*x;
    return (c1 + x2*(c2 + x2*(c3 + c4*x2)));
}

const float invtwopi=0.1591549f;
const float threehalfpi=4.7123889f;
const float quarterpi=0.7853982f;

float FTAcos(float angle){
    //clamp to the range 0..2pi
    angle=angle-floorf(angle*invtwopi)*TWOPI;
    angle=angle>0.f?angle:-angle;
    
    if(angle<PIOVERTWO) return cos_52s(angle);
    if(angle<PI) return -cos_52s(PI-angle);
    if(angle<threehalfpi) return -cos_52s(angle-PI);
    return cos_52s(TWOPI-angle);
}
float FTAsin(float angle){
    return FTAcos(PIOVERTWO-angle);
}



