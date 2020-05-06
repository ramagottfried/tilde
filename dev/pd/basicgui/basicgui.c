#include "m_pd.h"
#include "g_canvas.h"

typedef struct _basicgui
{
    t_object    ob;
    t_glist     *glist;
    char        io_tag[256];
    int         height, width, exists;
    
} t_basicgui;

static t_class *basicgui_class;
t_widgetbehavior basicgui_widget;


static void basicgui_getrect(t_gobj *z, t_glist *glist,int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_basicgui *x = (t_basicgui *)z;
    
    *xp1 = text_xpix(&x->ob, glist);
    *yp1 = text_ypix(&x->ob, glist);
    *xp2 = *xp1 + x->width;
    *yp2 = *yp1 + x->height;
    x->glist = glist;
}

static void basicgui_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_basicgui *x = (t_basicgui *)z;
    sys_vgui(".x%lx.c move %lxBOX %d %d\n", glist_getcanvas(glist), z, dx, dy );
    x->ob.te_xpix += dx;
    x->ob.te_ypix += dy;
    
    int x1, x2, y1, y2;
    basicgui_getrect(z, glist, &x1, &y1, &x2, &y2);
    
    t_object *ob = pd_checkobject(&x->ob.te_pd);
    if (ob){
        glist_drawiofor(glist, ob, 0, x->io_tag, x->ob.te_xpix, x->ob.te_ypix, x2, y2);
        canvas_fixlinesfor(glist, &x->ob);
    }

}

static void basicgui_select(t_gobj *z, t_glist *glist, int state)
{}

static void basicgui_activate(t_gobj *z, t_glist *glist, int state)
{}

static void basicgui_delete(t_gobj *z, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    t_basicgui *x = (t_basicgui *)z;
    t_object *ob = pd_checkobject(&x->ob.te_pd);
    
    if(canvas->gl_editor)
    {
        sys_vgui(".x%lx.c delete %lxBOX \n", glist_getcanvas(glist), z );
    }
    
    if(ob && glist_isvisible(glist))
    {
        glist_eraseiofor(glist, ob, x->io_tag);
    }
    
    canvas_deletelinesfor(glist, (t_text *)z);

//    canvas_deletelinesfor(canvas, ob);
    printf("%s end %p \n", __FUNCTION__, z);

}

void basicgui_drawElements(t_basicgui *x, t_gobj *z, t_glist *glist, int firstime)
{
    int x1, x2, y1, y2;
    basicgui_getrect(z, glist, &x1, &y1, &x2, &y2);
    if(firstime)
    {
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline \"blue\" -fill \"\" -tags %lxBOX \n", glist_getcanvas(glist), x1, y1, x2, y2, z);
    }
    else
    {
        if (glist_isvisible(glist))
            sys_vgui(".x%lx.c coords %lxBOX %d %d %d %d \n", glist_getcanvas(glist), z, x1, y1, x2, y2);
    }
    
    t_object *ob = pd_checkobject(&x->ob.te_pd);
    if (ob){
        glist_drawiofor(glist, ob, firstime, x->io_tag, x1, y1, x2, y2);
    }
    if (firstime) /* raise cords over everything else */
        sys_vgui(".x%lx.c raise cord\n", glist_getcanvas(glist));
    
    x->exists = 1;
    
}

void basicgui_vis(t_gobj *z, t_glist *glist, int state)
{
    //printf("%s %p state %d glist %p canvas %p\n",  __func__, (t_basicgui *)z, state, glist, glist_getcanvas(glist));
    t_basicgui *x = (t_basicgui *)z;

    if(state)
    {
        basicgui_drawElements(x, z, glist, state);
    }
    else
    {
        if(x->exists)
        {
            basicgui_delete(z, glist);
            x->exists = 0;
        }
    }
   
    
}

void basicgui_save(t_gobj *z, t_binbuf *b)
{
    t_basicgui *x = (t_basicgui *)z;
    binbuf_addv(b, "ssiis;", gensym("#X"),gensym("obj"), (t_int)x->ob.te_xpix, (t_int)x->ob.te_ypix, gensym("basicgui"));

}


void basicgui_bang(t_basicgui *x)
{
    int x1, x2, y1, y2;
    basicgui_getrect((t_gobj *)x, x->glist, &x1, &y1, &x2, &y2);
    post("%x %s x1 %d y1 %d x2 %d y2 %d", x, __func__, x1, y1, x2, y2);
    post("text pix x %d y %d", text_xpix(&x->ob, x->glist), text_ypix(&x->ob, x->glist));
}

void *basicgui_new(void)
{
    t_basicgui *x = (t_basicgui *)pd_new(basicgui_class);
    if(x)
    {
        x->glist = (t_glist *)canvas_getcurrent();
        printf("%s glist %p\n", __FUNCTION__, x->glist);
        x->height = 10;
        x->width = 30;
        sprintf(x->io_tag, "%lxIOTAG", (long)x);
    }
    return (void *)x;
}

void basicgui_setup(void) {

    basicgui_class = class_new(gensym("basicgui"),
			       (t_newmethod)basicgui_new,
			       0,
			       sizeof(t_basicgui),
			       CLASS_DEFAULT,
			       0);
    
    class_addbang(basicgui_class, basicgui_bang);
    
    basicgui_widget.w_getrectfn = basicgui_getrect;
    basicgui_widget.w_displacefn = basicgui_displace;
    basicgui_widget.w_deletefn = basicgui_delete;
    basicgui_widget.w_visfn = basicgui_vis;
    basicgui_widget.w_selectfn = NULL;
    basicgui_widget.w_activatefn = NULL;
    basicgui_widget.w_clickfn = NULL;

    
    class_setsavefn(basicgui_class, basicgui_save);
    class_setwidget(basicgui_class, &basicgui_widget);
    
}
