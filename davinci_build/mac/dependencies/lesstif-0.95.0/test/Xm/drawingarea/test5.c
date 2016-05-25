/*
** Generated by X-Designer 
*/
/*
**LIBS: -lXm -lXt -lX11
*/

#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>

#include <Xm/Xm.h>
#include <Xm/DialogS.h>
#include <Xm/DrawingA.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <stdio.h>
#include <stdlib.h>

extern void da_input ();
extern void da_resize ();
extern void da_expose ();
extern Boolean aardvarkConverter ();
extern Boolean objStringConverter ();
Widget appshell = (Widget) NULL;
Widget rowcol = (Widget) NULL;
Widget da = (Widget) NULL;
Widget text = (Widget) NULL;


GC gc=0;

#define LINE_WIDTH 10

void
create_gc(w)
	Widget          w;
{
	XGCValues       values;
	XColor          screen_def, exact_def;
	Display        *display = XtDisplay(w);
	Colormap        cmap = XDefaultColormapOfScreen(XtScreen(w));
	int             mask = 0;

	if (gc != 0)
		return;
	/* Allocate read-only colour cell for colour `red' */
	if (XAllocNamedColor(display, cmap, "red", &screen_def, &exact_def)
	    != 0) {
		/*
		 * Put the pixel value for red into the GC, ready for drawing
		 * operations
		 */
		values.foreground = screen_def.pixel;
		mask = GCForeground;
	}
	values.line_width=LINE_WIDTH;
	mask|=GCLineWidth;
	gc = XCreateGC(display, XtWindow(w), mask, &values);
}
static int      input_x = 0;
static int      input_y = 0;

void 
da_expose(w, client_data, call_data)
	Widget          w;
	XtPointer       client_data;
	XtPointer       call_data;
{
	Dimension       width, height, margin_width, margin_height;
	int             x_origin, y_origin;
	int             arc_width, arc_height;
	int             line_width;
	char           *line_width_s;
	unsigned char   unit_type;
	XmTextFieldWidget line_width_t = (XmTextFieldWidget) client_data;

	/* Call a routine to create a Graphics Context */
	create_gc(w);
	/* First get the various dimensions */
	XtVaGetValues(w,
		      XmNwidth, &width,
		      XmNheight, &height,
		      XmNmarginWidth, &margin_width,
		      XmNmarginHeight, &margin_height,
		      XmNunitType, &unit_type,
		      0);
	/* Convert values to pixels */
	width = XmConvertUnits(w, XmHORIZONTAL, (int) unit_type,
			       width, XmPIXELS);
	height = XmConvertUnits(w, XmVERTICAL, (int) unit_type,
				height, XmPIXELS);
	margin_width = XmConvertUnits(w, XmHORIZONTAL, (int) unit_type,
				      margin_width, XmPIXELS);
	margin_height = XmConvertUnits(w, XmVERTICAL, (int) unit_type,
				       margin_height, XmPIXELS);
	/* Obtain user's required line width in millimetres */
	line_width_s = XmTextFieldGetString((Widget)line_width_t);
	/* Convert to pixels */
	line_width = XmConvertUnits(w, XmHORIZONTAL,
			      Xm100TH_MILLIMETERS, 100 * atoi(line_width_s),
				    XmPIXELS);
	XtFree((char *) line_width_s);
	/* Calculate arc sizes */
	x_origin = margin_width + (line_width / 2);
	y_origin = margin_height + (line_width / 2);
	if (input_x < 0
	    || input_y < 0) {
		arc_width = width - x_origin * 2;
		arc_height = height - y_origin * 2;
	} else {
		arc_width = (input_x - x_origin) * 2;
		arc_height = (input_y - y_origin) * 2;
	}
	/* Don't draw 0 or negatively sized circles. */
	if (arc_width < line_width
	    || arc_height < line_width)
		return;
	/* Set the line width in the GC */
	XSetLineAttributes(XtDisplay(w), gc, line_width, LineSolid, CapButt,
			   JoinMiter);
	/* Draw the Arc */
	XDrawArc(XtDisplay(w), XtWindow(w), gc, x_origin, y_origin, arc_width,
		 arc_height, 0, 360 * 64);
}

void 
da_resize(w, client_data, call_data)
	Widget          w;
	XtPointer       client_data;
	XtPointer       call_data;
{
	if (XtIsRealized(w))
		XClearArea(XtDisplay(w), XtWindow(w), 0, 0, 0, 0, True);
}

void 
da_input(w, client_data, call_data)
	Widget          w;
	XtPointer       client_data;
	XtPointer       call_data;
{
	XEvent         *event = ((XmDrawingAreaCallbackStruct *) call_data)->event;
	/* Simply set the global variables, and redraw the circle */
	if (event->type == ButtonPress
	    || event->type == ButtonRelease) {
		input_x = event->xbutton.x;
		input_y = event->xbutton.y;
		XClearArea(XtDisplay(w), XtWindow(w), 0, 0, 0, 0, True);
	} else if (event->type == MotionNotify) {
		/* If we go negative default circle will get drawn */
		input_x = event->xmotion.x;
		input_y = event->xmotion.y;
		XClearArea(XtDisplay(w), XtWindow(w), 0, 0, 0, 0, True);
	}
}

void create_appshell (display, app_name, app_argc, app_argv)
Display *display;
char *app_name;
int app_argc;
char **app_argv;
{
	Widget children[2];      /* Children to manage */
	Arg al[64];                    /* Arg List */
	register int ac = 0;           /* Arg Count */

	XtSetArg(al[ac], XmNallowShellResize, TRUE); ac++;
	XtSetArg(al[ac], XmNtitle, "Drawing Area (8cm square)"); ac++;
	XtSetArg(al[ac], XmNargc, app_argc); ac++;
	XtSetArg(al[ac], XmNargv, app_argv); ac++;
	appshell = XtAppCreateShell ( app_name, "XApplication", applicationShellWidgetClass, display, al, ac );
	ac = 0;
	rowcol = XmCreateRowColumn ( appshell, "rowcol", al, ac );
	XtSetArg(al[ac], XmNwidth, 8000); ac++;
	XtSetArg(al[ac], XmNheight, 8000); ac++;
	XtSetArg(al[ac], XmNunitType, Xm100TH_MILLIMETERS); ac++;
	XtSetArg(al[ac], XmNresizePolicy, XmRESIZE_GROW); ac++;
	da = XmCreateDrawingArea ( rowcol, "da", al, ac );
	ac = 0;
	text = XmCreateTextField ( rowcol, "text", al, ac );
	XmTextFieldSetString ( text, "1" );
	 XtOverrideTranslations (da, XtParseTranslationTable(
	    "<BtnDown>:DrawingAreaInput()\n\
	    <BtnMotion>:DrawingAreaInput()"));
	XtAddCallback (da, XmNinputCallback, da_input,NULL);
	XtAddCallback (da, XmNresizeCallback, da_resize,NULL);
	XtAddCallback (da, XmNexposeCallback, da_expose,(XtPointer)text);
	children[ac++] = da;
	children[ac++] = text;
	XtManageChildren(children, ac);
	ac = 0;
	XtManageChild ( rowcol);
}



XtAppContext app_context;
Display *display;       /*  Display             */

int main (argc,argv)
int    argc;
char            **argv;
{
	XtSetLanguageProc ( (XtAppContext) NULL, (XtLanguageProc) NULL, (XtPointer) NULL );
	XtToolkitInitialize ();
	app_context = XtCreateApplicationContext ();
	display = XtOpenDisplay (app_context, NULL, argv[0], "XApplication",
				 NULL, 0, &argc, argv);
	if (!display)
	{
	    printf("%s: can't open display, exiting...\n", argv[0]);
	    exit (-1);
	}
	create_appshell ( display, argv[0], argc, argv );
	XtRealizeWidget (appshell);
  
{
static XtWidgetGeometry Expected[] = {
   CWWidth | CWHeight            ,  506,  322,  248,  282, 0,0,0, /* rowcol */
   CWWidth | CWHeight | CWX | CWY,   99,   99, 7986, 7986, 0,0,0, /* da */
   CWWidth | CWHeight | CWX | CWY,    3,  248,  242,   31, 0,0,0, /* text */
};
int width, height, x, y;
/* toplevel should be replaced with to correct applicationShell */
width = XmConvertUnits(appshell, XmHORIZONTAL,Xm100TH_MILLIMETERS, 8000, XmPIXELS);
Expected[0].width = width + 6;
Expected[2].width = width;
width = XmConvertUnits(appshell, XmHORIZONTAL,XmPIXELS, width, Xm100TH_MILLIMETERS);
x = XmConvertUnits(appshell, XmHORIZONTAL,XmPIXELS, 3, Xm100TH_MILLIMETERS);
height = XmConvertUnits(appshell, XmVERTICAL,Xm100TH_MILLIMETERS, 8000, XmPIXELS);
Expected[0].height = height + 34 + 6;
Expected[2].y = height + 6;
height = XmConvertUnits(appshell, XmVERTICAL,XmPIXELS, height, Xm100TH_MILLIMETERS);
y = XmConvertUnits(appshell, XmVERTICAL,XmPIXELS, 3, Xm100TH_MILLIMETERS);
printf("%ix%i %ix%i %ix%i\n", 
	WidthOfScreen(XtScreen(appshell)), HeightOfScreen(XtScreen(appshell)),
	WidthMMOfScreen(XtScreen(appshell)), HeightMMOfScreen(XtScreen(appshell)),
	width, height);
Expected[1].x = x;
Expected[1].y = y;
Expected[1].width = width;
Expected[1].height = height;
PrintDetails(appshell, Expected);
}
  LessTifTestMainLoop(appshell);

	exit (0);
}
