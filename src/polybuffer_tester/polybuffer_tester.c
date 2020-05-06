/* 
 polybuffer_tester
 Demonstration of the polybuffer API / global notification mechanism
 ej - Copyright 2010 - Cycling '74
 */

#include "ext.h"
#include "ext_obex.h"
#include "ext_globalsymbol.h"
#include "buffer.h"
#include "jpatcher_utils.h"
#include "ext_polybuffer.h"

typedef struct _polybuffer_tester{
    t_object	p_obj;
	t_symbol	*p_name;
	t_object	*p_object;
	void		*p_outlet;
} t_polybuffer_tester;

// prototypes
void *polybuffer_tester_new(t_symbol *s, long argc, t_atom *argv);
void polybuffer_tester_free(t_polybuffer_tester *x);
void polybuffer_tester_assist(t_polybuffer_tester *x, void *b, long m, long a, char *s);
void polybuffer_tester_getcount(t_polybuffer_tester *x);
void polybuffer_tester_set(t_polybuffer_tester *x, t_symbol *s);
void polybuffer_tester_doset(t_polybuffer_tester *x, t_symbol *s, short ac, t_atom *av);
t_max_err polybuffer_tester_notify(t_polybuffer_tester *x, t_symbol *s, t_symbol *msg, void *sender, void *data);

// class statics/globals
static t_class	*s_polybuffer_tester_class;



/************************************************************************************/
int main(void)
{
	t_class	*c;
	
	common_symbols_init();
	
	c = class_new("polybuffer_tester",
				  (method)polybuffer_tester_new,
				  (method)polybuffer_tester_free,
				  sizeof(t_polybuffer_tester),
				  (method)NULL,
				  A_GIMME,
				  0L);

	class_addmethod(c, (method)polybuffer_tester_getcount,		"getcount", 	0);
	class_addmethod(c, (method)polybuffer_tester_assist,		"assist",		A_CANT, 0);
	class_addmethod(c, (method)polybuffer_tester_set,			"set",			A_SYM, 0);
	
	class_addmethod(c, (method)polybuffer_tester_notify,		"notify",		A_CANT, 0);

	class_register(CLASS_BOX, c);
	s_polybuffer_tester_class = c;
	
	return 0;
}

/************************************************************************************/
void *polybuffer_tester_new(t_symbol *s, long argc, t_atom *argv)
{
	t_polybuffer_tester *x = NULL;

	if (x = (t_polybuffer_tester *)object_alloc(s_polybuffer_tester_class)) {
		x->p_outlet = outlet_new(x, 0L);
		x->p_object = NULL;
		
		if (argc && atom_gettype(argv) == A_SYM)
			defer_low(x, (method)polybuffer_tester_doset, atom_getsym(argv), 0, NULL);
	}
	
	return (x);
}

void polybuffer_tester_free(t_polybuffer_tester *x)
{
	// detaching as polybuffer_tester is being freed.
	if (x->p_name && x->p_name->s_name)
		globalsymbol_dereference((t_object *)x, x->p_name->s_name, "polybuffer");
}

/************************************************************************************/
void polybuffer_tester_assist(t_polybuffer_tester *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET)
		strcpy(s, "Message in");
	else if (m == ASSIST_OUTLET)
		strcpy(s, "Message out");
}

void polybuffer_tester_getcount(t_polybuffer_tester *x)
{
	if (x->p_object) {
		t_atom a;
		long count = object_attr_getlong(x->p_object, _sym_count);
	
		atom_setlong(&a, count);
		outlet_anything(x->p_outlet, _sym_count, 1, &a);
	}
}

void polybuffer_tester_set(t_polybuffer_tester *x, t_symbol *s)
{
	// doesn't really need to be defered
	polybuffer_tester_doset(x, s, 0, NULL);
}

void polybuffer_tester_doset(t_polybuffer_tester *x, t_symbol *s, short ac, t_atom *av)
{
	if (s != _sym_nothing && s != x->p_name) {
		t_symbol *old_name = x->p_name;
		
		if (s && s->s_name) {
			if (x->p_object = polybuffer_getobject(s)) {	// returns NULL if the polybuffer doesn't exist
				x->p_name = s;
				globalsymbol_reference((t_object *)x, x->p_name->s_name, "polybuffer");
			} else {
				object_error((t_object *)x, "%s is not a valid polybuffer", s->s_name);
				return;
			}
		}
		
		if (old_name && old_name->s_name)
			globalsymbol_dereference((t_object *)x, old_name->s_name, "polybuffer");
	}
}

t_max_err polybuffer_tester_notify(t_polybuffer_tester *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
	if (msg == gensym("count_will_change")) {
		object_post((t_object *)x, "polybuffer_tester is notified, that the number of buffers hold by %s is about to change", x->p_name->s_name);
	} else if (msg == gensym("count_changed")) {	// count_changed notification
		long count = object_attr_getlong(data, _sym_count);
		
		if (x->p_name && x->p_name->s_name)
			object_post((t_object *)x, "polybuffer_tester is notified, that %s now holds %d buffers.", x->p_name->s_name, count);
	} else if (msg == gensym("polybuffer_free")) {
		t_symbol *name = object_attr_getsym(data, _sym_name);
		
		if (name && name->s_name && name == x->p_name) {
			object_post((t_object *)x, "polybuffer_tester is notified that %s is being freed.", x->p_name->s_name);
			globalsymbol_dereference((t_object *)x, x->p_name->s_name, "polybuffer");	// detaching
			x->p_object = NULL;
			x->p_name = NULL;
		}
	} else if (msg == gensym("child_modified")) {
		t_buffer *b = (t_buffer *)data;
		if (x->p_name && x->p_name->s_name && b->b_name && b->b_name->s_name)
			post("polybuffer %s tells me that buffer~ %s was modified", x->p_name->s_name, b->b_name->s_name);
	}
	
	return MAX_ERR_NONE;
}
