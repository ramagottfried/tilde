
#include "m_pd.h"

static t_class *omessage_class;

typedef struct _omessage {
  t_object  x_obj;
} t_omessage;

void omessage_bang(t_omessage *x)
{
    post("Hello world !!");
}

void *omessage_new(void)
{
    t_omessage *x = (t_omessage *)pd_new(omessage_class);
    return (void *)x;
}

void omessage_setup(void) {

    omessage_class = class_new(gensym("omessage"),
			       (t_newmethod)omessage_new,
			       0,
			       sizeof(t_omessage),
			       CLASS_DEFAULT,
			       0);

    class_addbang(omessage_class, omessage_bang);
}
