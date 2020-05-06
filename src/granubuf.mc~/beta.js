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

function beta( a, b )
{
	return Math.exp( gamma(a) + gamma(b) - gamma(a+b) );
}


var len = 100;
var maxIdx = len - 1;

function pdf(x, a, b, betaCoef )
{
	return (Math.pow(x, a - 1 ) * Math.pow(1 - x, b - 1)) / betaCoef;
}


function normPDFarray(a, b, betaCoef, maxVal )
{
	var normalized = [];
	
	for( var i = 0; i < len; ++i)
	{
		var x = i / maxIdx;
		var norm = pdf(x, a, b, betaCoef) / maxVal;
		normalized.push( norm );
	}
	
	return normalized;
}




function getBetaPDF(a, b)
{
	var betaCoef = beta(a,b);

	if( a > 1 && b > 1 )
	{
		var mode = (a - 1) / (a + b - 2);
		mode = Math.round(mode * maxIdx) / maxIdx;
		var maxVal = pdf(mode, a, b, betaCoef);
		var arr = normPDFarray(a, b, betaCoef, maxVal);
		outlet(0, arr);

	}
	else if( a > 1 && b == 1 )
	{
		var mode = 1;
		var maxVal = pdf(mode, a, b, betaCoef);
		var arr = normPDFarray(a, b, betaCoef, maxVal);
		outlet(0, arr);
	}
	else if( a == 1 && b > 1 )
	{
		var mode = 0;
		var maxVal = pdf(mode, a, b, betaCoef);
		var arr = normPDFarray(a, b, betaCoef, maxVal);
		outlet(0, arr);
	}
	else if( a == 1 && b > 1 )
	{
		var mode = (maxIdx - 1) / maxIdx;
		var maxVal = pdf(mode, a, b, betaCoef);
		var arr = normPDFarray(a, b, betaCoef, maxVal);
		arr[maxIdx] = 1; // avoid inf
		outlet(0, arr);
	}

}

