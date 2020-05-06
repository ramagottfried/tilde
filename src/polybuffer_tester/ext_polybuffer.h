/* 
	ext_polybuffer.c
	API for access to a named polybuffer or the list of polybuffers
	ej - Copyright 2010 - Cycling '74
*/


#include "ext.h"


BEGIN_USING_C_LINKAGE


// PRIVATE METHODS ****************************************************************************


// PUBLIC METHODS ****************************************************************************

/**	Returns 1 when the symbol is bind to a polybuffer, returns 0 otherwise (coll, send, buffer~...)
 
 @ingroup polybuffer 
 @param	s		t_symbol that you want to search
 */
long polybuffer_symbol_is_polybuffer(t_symbol *s);

/**	Returns a polybuffer object. If the polybuffer doesn't exist, it returns NULL.
 
 @ingroup polybuffer 
 @param		name	t_symbol of the polybuffer
 @return			A polybuffer object (NULL if it doesn't exists)
 */
t_object *polybuffer_getobject(t_symbol *name);


END_USING_C_LINKAGE
