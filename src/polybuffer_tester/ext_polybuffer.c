/* 
	ext_polybuffer.c
	API for access to a named polybuffer or the list of polybuffers
	ej - Copyright 2010 - Cycling '74
*/

#include "ext.h"
#include "ext_obex.h"
#include "ext_strings.h"
#include "ext_polybuffer.h"


//public:
long polybuffer_symbol_is_polybuffer(t_symbol *s)
{
	if (s && s->s_thing && !NOGOOD(s->s_thing) && ob_sym(s->s_thing) == gensym("polybuffer"))
		return (1);
	else
		return (0);
}

t_object *polybuffer_getobject(t_symbol *name)
{
	t_object *rtn = NULL;
	
	if (polybuffer_symbol_is_polybuffer(name)) {
		rtn = (t_object *)(name->s_thing);
	}
	
	return (rtn);
}