
#ifndef __svg_lib__bezier_info__
#define __svg_lib__bezier_info__

//#include "gauss_legendre.h"

double getLineLength(double x1, double y1, double x2, double y2);
void deCasteljau( double t, const double *p, int pLen, double *pt);
double cubicLength(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);

/*
double computeQuadraticCurveLength(double z, int n, double x1, double y1, double x2, double y2, double x3, double y3);
double quadraticF(double t, double x1, double y1, double x2, double y2, double x3, double y3);
double base2(double t, double p1, double p2, double p3);
double computeCubicCurveLength(double z, int n, double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
double cubicF(double t, double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
double base3(double t, double p1, double p2, double p3, double p4);
*/

#endif /* defined(__svg_lib__bezier_info__) */
