#include "bezier_info.h"

//#include "gauss_legendre.h"
//static const double EPS = 1e-10;

double getLineLength(double x1, double y1, double x2, double y2)
{
    double x = x2 - x1;
    double y = y2 - y1;
    return (sqrt((x*x)+(y*y)));
}

void deCasteljau( double t, const double *p, int pLen, double *pt)
{
    // de Casteljau recursive midpoint method
    int i;
    int qLen = pLen-2;
    double q[ qLen ];
    for( i = 0; i < qLen; i++)
    {
        q[i] = p[i] + t * (p[i+2] - p[i]);
    }
    if( qLen == 2 )
    {
        memcpy(pt, q, sizeof(double) * 2);
    }
    else
        deCasteljau(t, q, qLen, pt);

}

double cubicLength(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
{
    double pts[] = { x1, y1, x2, y2, x3, y3, x4, y4 };
    
    double midPt[2];
    double prevMidPt[2] = { pts[0], pts[1] };
    
    double t = 0;
    int steps = 1000;
    double inc = 1.0 / steps;
    double len = 0;
    for (int i = 0; i <= steps; i++)
    {
        deCasteljau( t, pts, 8, midPt );
        
        if (i > 0)
        {
            len += getLineLength(midPt[0], midPt[1], prevMidPt[0], prevMidPt[1]);
        }
        memcpy(prevMidPt, midPt, sizeof(double) * 2);
        memset(midPt, 0, sizeof(double) * 2);

        t += inc;
    }
    return len;
}

