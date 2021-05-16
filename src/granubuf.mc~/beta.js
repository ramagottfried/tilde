// gamma function from https://github.com/substack/gamma.js/blob/master/index.jsw
// transliterated from the python snippet here:
// http://en.wikipedia.org/wiki/Lanczos_approximation

var g = 7;
var p = [
    0.99999999999980993,
    676.5203681218851,
    -1259.1392167224028,
    771.32342877765313,
    -176.61502916214059,
    12.507343278686905,
    -0.13857109526572012,
    9.9843695780195716e-6,
    1.5056327351493116e-7
];

var g_ln = 607/128;
var p_ln = [
    0.99999999999999709182,
    57.156235665862923517,
    -59.597960355475491248,
    14.136097974741747174,
    -0.49191381609762019978,
    0.33994649984811888699e-4,
    0.46523628927048575665e-4,
    -0.98374475304879564677e-4,
    0.15808870322491248884e-3,
    -0.21026444172410488319e-3,
    0.21743961811521264320e-3,
    -0.16431810653676389022e-3,
    0.84418223983852743293e-4,
    -0.26190838401581408670e-4,
    0.36899182659531622704e-5
];

// Spouge approximation (suitable for large arguments)
function lngamma(z) {

    if(z < 0) return Number('0/0');
    var x = p_ln[0];
    for(var i = p_ln.length - 1; i > 0; --i) x += p_ln[i] / (z + i);
    var t = z + g_ln + 0.5;
    return .5*Math.log(2*Math.PI)+(z+.5)*Math.log(t)-t+Math.log(x)-Math.log(z);
}

function gamma (z) {
    if (z < 0.5) {
        return Math.PI / (Math.sin(Math.PI * z) * gamma(1 - z));
    }
    else if(z > 100) return Math.exp(lngamma(z));
    else {
        z -= 1;
        var x = p[0];
        for (var i = 1; i < g + 2; i++) {
            x += p[i] / (z + i);
        }
        var t = z + g + 0.5;

        return Math.sqrt(2 * Math.PI)
            * Math.pow(t, z + 0.5)
            * Math.exp(-t)
            * x
        ;
    }
}
/*
function gamma(x) {
    var p = [0.99999999999980993, 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61502916214059, 12.507343278686905,
        -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
    ];
 
    var g = 7;
    if (x < 0.5) {
        return Math.PI / (Math.sin(Math.PI * x) * gamma(1 - x));
    }
 
    x -= 1;
    var a = p[0];
    var t = x + g + 0.5;
    for (var i = 1; i < p.length; i++) {
        a += p[i] / (x + i);
    }
 
    return Math.sqrt(2 * Math.PI) * Math.pow(t, x + 0.5) * Math.exp(-t) * a;
}
*/

function beta( a, b )
{
	return Math.exp( lngamma(a) + lngamma(b) - lngamma(a+b) );
}


var len = 100;
var maxIdx = len - 1;
var m_incr = 1 / len;


function betaMode(a, b)
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
        post("unmatched beta mode %f %f returning 0\n", a, b);
        return 0;
    }
}



function betaNumerator(x, a, b)
{
    var num = Math.pow(x, a-1) * Math.pow(1-x, b-1);    
    return num == Infinity ? 1 : num;
}


function getBetaScalar( a, b, stepsize)
{
    
    // reusing denominator: (betaNum / betaDen) == betaPDF
    var betaDenominator = beta(a, b);
    
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
        post("unknown situation", a, b, '\n');
    
    return 0;
    
}


var min_coef = 0.000001;

function betaDist(a,b)
{
	a = a <= min_coef ? min_coef : a;
	b = b <= min_coef ? min_coef : b;
		
    var  m_wind_norm_coef = getBetaScalar(a, b, m_incr);
	var normalized = [];
	
	for( var i = 0; i < len; ++i)
	{
		var phase = i * m_incr;

        var v = betaNumerator(phase, a, b) * m_wind_norm_coef;
		if( v < 0.000001 ) v = 0;
		if( v > 1 ) v = 1;

		normalized.push( v );
	}

	outlet(0, normalized);

}

