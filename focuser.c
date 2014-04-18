#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iup/iup.h>
#include <iup/iupcontrols.h>
#include <iup/iup_pplot.h>
#include <cd/cd.h>
#include <cd/cdiup.h>
#include <im/im.h>
#include <im/im_image.h>

Ihandle *tmr;
Ihandle* dlg;
imImage *image;
Ihandle *hst_plot;
Ihandle *lbl_status;
int *buf;
int idx = 0;
float plot_x = 0.0;
int capture_width, capture_height;

Display *dpy;
Window root;

static int cb_canvas_repaint(Ihandle* iup_canvas)
{
    cdCanvas* cd_canvas = (cdCanvas*)IupGetAttribute(iup_canvas, "cdCanvas");

    if(!cd_canvas)
        return IUP_DEFAULT;

    cdCanvasActivate(cd_canvas);
    cdCanvasClear(cd_canvas);

    if(!image)
        return IUP_DEFAULT;

    imcdCanvasPutImage(cd_canvas, image, 0, 0, 100, 100, 0, 0, 0, 0);

    cdCanvasFlush(cd_canvas);

    return IUP_DEFAULT;
}

static int cb_canvas_map(Ihandle* iup_canvas)
{
    cdCanvas* cd_canvas = cdCreateCanvas(CD_IUP, iup_canvas);
    IupSetAttribute(IupGetDialog(iup_canvas), "cdCanvas", (char*)cd_canvas);
    return IUP_DEFAULT;
}

static int cb_dlg_close(Ihandle* iup_dialog)
{
    cdCanvas* cd_canvas = (cdCanvas*)IupGetAttribute(iup_dialog, "cdCanvas");

    if(cd_canvas) cdKillCanvas(cd_canvas);

    IupSetAttribute(iup_dialog, "cdCanvas", NULL);

    return IUP_CLOSE;
}

int cb_hst_btn(Ihandle *self, int btn, int pressed, float x, float y, char *status)
{
    return IUP_DEFAULT;
}

int cb_hst_motion(Ihandle *self, int x, int y, char *status)
{
    return IUP_DEFAULT;
}

void hfd_calc(int pos_x, int pos_y, int width, int height, float *hfd)
{
    int x, y, d_x, d_y;
    int pixel_val;
    long pixel, offset;
    long average;
    long c_x, c_y;
    float mass, flux;

    if((pos_x -= width) < 0)
        pos_x = 0;

    /* capture screenshot from the screen */
    XImage *x_img = XGetImage(dpy, root, pos_x, pos_y, width, height, XAllPlanes(), ZPixmap);

    average = 0;
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            pixel = XGetPixel(x_img, x, y);
            offset = y * width + x;
            /* copy data to imImage structure which will display in the application */
            ((unsigned char *)(image->data[0]))[offset] = (pixel >> 16);
            ((unsigned char *)(image->data[1]))[offset] = ((pixel & 0x00ff00) >> 8);
            ((unsigned char *)(image->data[2]))[offset] = (pixel & 0x0000ff);
            /* copy data to buf to calc hfd */
            buf[offset] = ((unsigned char *)(image->data[0]))[offset] +
                          ((unsigned char *)(image->data[1]))[offset] +
                          ((unsigned char *)(image->data[2]))[offset] ;
            average += buf[offset];
        }
    }

    XDestroyImage(x_img);

    average /= (width * height);

    mass = 0;
    c_x = 0;
    c_y = 0;
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            offset = y * width + x;
            pixel_val = buf[offset] - average;
            pixel_val = pixel_val < 0 ? 0 : pixel_val;
            c_x += pixel_val * x;
            c_y += pixel_val * y;

            mass += pixel_val;
        }
    }

    mass = mass == 0 ? 1 : mass;
    c_x /= mass;
    c_y /= mass;

    flux = 0;
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            offset = y * width + x;
            pixel_val = buf[offset] - average;
            pixel_val = pixel_val < 0 ? 0 : pixel_val;

            d_x = abs(c_x - x);
            d_y = abs(c_y - y);
            flux += sqrt(d_x * d_x + d_y * d_y) * pixel_val;
        }
    }

    *hfd = (flux / mass) * 2;

    cb_canvas_repaint(dlg);
}

void hfd_update(void)
{
    char str[50];
    int pos_x, pos_y;
    float hfd;

    IupGetIntInt(dlg, "SCREENPOSITION", &pos_x, &pos_y);
    printf("pos_x = %d, pos_y = %d\n", pos_x, pos_y);

    hfd_calc(pos_x, pos_y, capture_width, capture_height, &hfd);

    snprintf(str, 50, "HFD=%f", hfd);
    IupSetAttribute(lbl_status, "TITLE", str);

    IupPPlotAddPoints(hst_plot, idx, &plot_x, &hfd, 1);
    if(++plot_x > 100) {
        IupSetInt(hst_plot, "DS_REMOVE", 0);
        IupSetInt(hst_plot, "AXS_XMAX", plot_x + 50);
    }
    IupSetAttribute(hst_plot, "REDRAW", "YES");
}

int cb_tmr(Ihandle *self)
{
    hfd_update();
    return IUP_DEFAULT;
}

int cb_move(Ihandle *self, int x, int y)
{
    hfd_update();
    return IUP_DEFAULT;
}

int cb_lst_size(Ihandle *self)
{
    int item;

    imImageDestroy(image);
    free(buf);

    item = IupGetInt(self, "VALUE");
    if(item == 1) {
        capture_width = capture_height = 80;
    } else if(item == 2) {
        capture_width = capture_height = 100;
    } else if(item == 3) {
        capture_width = capture_height = 150;
    } else if(item == 4) {
        capture_width = capture_height = 200;
    }

    image = imImageCreate(capture_width, capture_height, IM_RGB, IM_BYTE);
    buf = (int *)malloc(sizeof(int) * capture_width * capture_height);
    if(buf == NULL) {
        fprintf(stderr, "Fail to allocate memery.\n");
        return IUP_CLOSE;
    }

    hfd_update();
    return IUP_DEFAULT;
}

static Ihandle* create_dialog(void)
{
    Ihandle *iup_dialog, *iup_canvas;
    Ihandle *lst_size;

    iup_canvas = IupCanvas(NULL);
    IupSetCallback(iup_canvas, "ACTION", (Icallback)cb_canvas_repaint);
    IupSetCallback(iup_canvas, "MAP_CB", (Icallback)cb_canvas_map);
    IupSetAttribute(iup_canvas, "RASTERSIZE", "100x100");

    lst_size = IupList(NULL);
    IupSetAttributes(lst_size, "1=80x80,2=100x100,3=150x150,4=200x200,DROPDOWN=YES,VALUE=2,RASTERSIZE=100x");
    IupSetCallback(lst_size, "VALUECHANGED_CB", (Icallback)cb_lst_size);

    lbl_status = IupLabel("I am a Lable.");
    IupSetAttribute(lbl_status, "SIZE", "100");

    hst_plot = IupPPlot();
    IupSetAttribute(hst_plot, "RASTERSIZE", "200x100");
    IupSetAttribute(hst_plot, "EXPAND", "NO");
    IupSetAttribute(hst_plot, "USE_CONTEXTPLUS", "YES");
    IupSetAttribute(hst_plot, "BGCOLOR", "0 0 0");
    IupSetAttribute(hst_plot, "GRID", "HORIZONTAL");
    IupSetAttribute(hst_plot, "GRIDCOLOR", "100 100 100");
    IupSetAttribute(hst_plot, "MARGINTOP", "2");
    IupSetAttribute(hst_plot, "MARGINRIGHT", "2");
    IupSetAttribute(hst_plot, "MARGINBOTTOM", "2");
    IupSetAttribute(hst_plot, "MARGINLEFT", "2");
    IupSetAttribute(hst_plot, "AXS_XAUTOMAX", "NO");
    IupSetAttribute(hst_plot, "AXS_YAUTOMAX", "NO");
    IupSetAttribute(hst_plot, "AXS_YAUTOMIN", "NO");
    IupSetInt(hst_plot, "AXS_XMAX", 200);
    IupSetInt(hst_plot, "AXS_YMAX", 50);
    IupSetInt(hst_plot, "AXS_YMIN", 0);
    IupSetCallback(hst_plot, "MOTION_CB", (Icallback)cb_hst_motion);
    IupSetCallback(hst_plot, "PLOTBUTTON_CB", (Icallback)cb_hst_btn);
    IupPPlotBegin(hst_plot, 0);
    idx = IupPPlotEnd(hst_plot);
    IupSetAttribute(hst_plot, "DS_COLOR", "255 0 0");
    IupSetAttribute(hst_plot, "DS_LINEWIDTH", "1.5");

    tmr = IupTimer();
    IupSetAttribute(tmr, "TIME", "100");
    IupSetCallback(tmr, "ACTION_CB", (Icallback)cb_tmr);

    iup_dialog = IupDialog(
                     IupVbox(
                         IupHbox(iup_canvas, hst_plot, NULL),
                         IupSetAttributes(IupHbox(lst_size, IupSetAttributes(IupFill(), "SIZE=5"), lbl_status, NULL), "ALIGNMENT=ACENTER"),
                         NULL)
                 );

    IupSetAttribute(iup_dialog, "RESIZE", "NO");
    IupSetAttribute(iup_dialog, "TITLE", "HFD Focuser");
    IupSetCallback(iup_dialog, "CLOSE_CB", (Icallback)cb_dlg_close);
    IupSetCallback(iup_dialog, "MOVE_CB", (Icallback)cb_move);

    return iup_dialog;
}


int main(int argc, char* argv[])
{
    IupOpen(&argc, &argv);
    IupPPlotOpen();

    capture_width = 100;
    capture_height = 100;

    dpy = XOpenDisplay(NULL);
    root = DefaultRootWindow(dpy);

    image = imImageCreate(capture_width, capture_height, IM_RGB, IM_BYTE);

    buf = (int *)malloc(sizeof(int) * capture_width * capture_height);
    if(buf == NULL) {
        fprintf(stderr, "Fail to allocate memery.\n");
        return -1;
    }

    dlg = create_dialog();

    IupShow(dlg);
    IupSetAttribute(tmr, "RUN", "YES");

    IupMainLoop();
    imImageDestroy(image);
    IupDestroy(tmr);
    IupDestroy(dlg);
    IupClose();

    return 0;
}
