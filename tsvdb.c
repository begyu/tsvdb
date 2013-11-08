/*
 * $Id: tcsvdb.c,v 0.6.4 2013/11/08 $
 */

#define VERSION "0.6.4"

#ifdef XCURSES
#include <xcurses.h>
# define FNAME  "./new.tsv"
#else
# define FNAME  ".\\new.tsv"
#ifdef NCURSES
#include <curses.h>
#else
/*#include <pdcurses.h>*/
#include <curses.h>
#endif
#endif

/*#include <ctype.h>*/
/*#include <stdio.h>*/
/*#include <stdlib.h>*/
/*#include <string.h>*/
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <wchar.h>
/*#include <getopt.h>*/
#include <sys/stat.h>

#ifndef MSDOS
  #include <libgen.h>
#endif

/****REGEXP****/
/* http://cesanta.com */
#include "slre.c"

static char *slre_replace(const char *regex, const char *buf,
                          const char *sub) {
  char *s = NULL;
  int n, n1, n2, n3, s_len, len = strlen(buf);
  struct slre_cap cap = { NULL, 0 };

  do {
    s_len = s == NULL ? 0 : strlen(s);
    if ((n = slre_match(regex, buf, len, &cap, 1)) > 0) {
      n1 = cap.ptr - buf, n2 = strlen(sub),
         n3 = &buf[n] - &cap.ptr[cap.len];
    } else {
      n1 = len, n2 = 0, n3 = 0;
    }
    s = (char *) realloc(s, s_len + n1 + n2 + n3 + 1);
    memcpy(s + s_len, buf, n1);
    memcpy(s + s_len + n1, sub, n2);
    memcpy(s + s_len + n1 + n2, cap.ptr + cap.len, n3);
    s[s_len + n1 + n2 + n3] = '\0';

    buf += n > 0 ? n : len;
    len -= n > 0 ? n : len;
  } while (len > 0);

  return s;
}
/*END_REGEXP*/


#ifdef A_COLOR
#define A_ATTR  (A_ATTRIBUTES ^ A_COLOR)  /* A_BLINK, A_REVERSE, A_BOLD */
#else
#define A_ATTR  (A_ATTRIBUTES)            /* standard UNIX attributes */
#endif

#define MAXSTRLEN  256
#define KEY_ESC    0x1b     /* Escape */

#ifdef NCURSES
#define CTL_LEFT        0x302
#define CTL_RIGHT       0x303
#define CTL_END         0x307
#endif

#define CTRL_C 0x03
#define CTRL_F 0x06
#define CTRL_V 0x16
#define CTRL_X 0x18
#define CTRL_U 0x15
#define CTRL_L 0x0C
#define CTRL_A 0x01
#define CTRL_B 0x02
#define CTRL_D 0x04
/*
//#define       ALT_U 0x1B5

//#define       ALT_L 0x1aC
*/

void subfunc1(void);
#define HELP subfunc1()
void edithelp(void);
#define EDITHLP edithelp()
void reghelp(void);
#define REGHLP reghelp()

bool regexp = FALSE;

/****DAT****/

#define MAXROWS  1000000
#define MAXCOLS  16

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define CTRL(x) ((x) & 0x1f)

char progname[MAXSTRLEN] = "";
char datfname[MAXSTRLEN] = "";
char fstr[MAXSTRLEN] = "";
char head[MAXSTRLEN] = "";
char clp[MAXSTRLEN] = "";
char *rows[MAXROWS+1];
char flags[MAXROWS+1];
char stru[MAXCOLS+1][MAXSTRLEN];
int beg[MAXCOLS+1];
int len[MAXCOLS+1];
int cols, reccnt, curr, field, curcol;
bool modified=FALSE;
bool ro = FALSE;
bool wr = FALSE;
bool cry = FALSE;
bool crypted = FALSE;
bool locked = FALSE;
int button;
bool ontop = FALSE;
bool bottom = FALSE;
bool pfind = FALSE;
bool regex = FALSE;
int unkeys[] = {KEY_RIGHT, '\n', '\n', '\t', KEY_END, KEY_UP, 0};
int unkeypos = -1;
int sortpos = 0;
bool filtered = FALSE;

#define TABCSEP '\t'
#define TABSSEP "\t"
#define COLCSEP ','
#define COLSSEP ","
#define SCLCSEP ';'
#define SCLSSEP ";"

#define RXFORW 0xFF
#define RXBACK 0xFE
#define WSTR (" Wait! ")
#define DISABLEDHOT ("CFNS")

char csep = TABCSEP;
char ssep[] = TABSSEP;

int casestr(char *, bool, bool);
int capfstr(char *, bool, bool);

#if 1
#include <errno.h>

char *itoa(int value, char *string, int radix)
{
  char tmp[33];
  char *tp = tmp;
  int i;
  unsigned v;
  int sign;
  char *sp;

  if (radix > 36 || radix <= 1)
  {
    errno = EDOM;
    return 0;
  }
  sign = (radix == 10 && value < 0);
  if (sign)
    v = -value;
  else
    v = (unsigned)value;
  while (v || tp == tmp)
  {
    i = v % radix;
    v = v / radix;
    if (i < 10)
      *tp++ = i+'0';
    else
      *tp++ = i + 'a' - 10;
  }
  if (string == 0)
    string = (char *)malloc((tp-tmp)+sign+1);
  sp = string;
  if (sign)
    *sp++ = '-';
  while (tp > tmp)
    *sp++ = *--tp;
  *sp = 0;
  return string;
}
#endif

#include <signal.h>

void siginthandler(int param)
{
    return;
}


/****DAT****/

typedef void (*FUNC)(void);

typedef struct 
{
    char *name; /* item label */
    FUNC  func; /* (pointer to) function */
    char *desc; /* function description */
} menu;

/* ANSI C function prototypes: */
/*
void    clsbody(void);
int     bodylen(void);
WINDOW *bodywin(void);

void    rmerror(void);
void    rmstatus(void);

void    titlemsg(char *msg);
void    bodymsg(char *msg);
void    errormsg(char *msg);
void    statusmsg(char *msg);

bool    keypressed(void);
int     getkey(void);
int     waitforkey(void);

void    DoExit(void);
void    domenu(menu *mp);
void    startmenu(menu *mp, char *title);

int     weditstr(WINDOW *win, char *buf, int field);
WINDOW *winputbox(WINDOW *win, int nlines, int ncols);
int     getstrings(char *desc[], char *buf[], int field, int length);
*/

#define editstr(s,f)           (weditstr(stdscr,s,f))
#define mveditstr(y,x,s,f)     (move(y,x)==ERR?ERR:editstr(s,f))
#define mvweditstr(w,y,x,s,f)  (wmove(w,y,x)==ERR?ERR:weditstr(w,s,f))

#define inputbox(l,c)          (winputbox(stdscr,l,c))
#define mvinputbox(y,x,l,c)    (move(y,x)==ERR?w:inputbox(l,c))
#define mvwinputbox(w,y,x,l,c) (wmove(w,y,x)==ERR?w:winputbox(w,l,c))


/********************************* tui.c ************************************/
/*
 * 'textual user interface'
 *
 * $Id: tui.c,v 1.34 2008/07/14 12:35:23 wmcbrine Exp $
 *
 * Author : P.J. Kunst <kunst@prl.philips.nl>
 * Date   : 25-02-93
 */

//#include <ctype.h>
//#include <curses.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <time.h>
//#include <locale.h>

void statusmsg(char *);
int waitforkey(void);
void rmerror(void);

#if defined(__unix) && !defined(__DJGPP__)
#include <unistd.h>
#endif

#ifdef A_COLOR
# define TITLECOLOR       1       /* color pair indices */
# define MAINMENUCOLOR    (2 | A_BOLD)
# define MAINMENUREVCOLOR (3 | A_BOLD | A_REVERSE)
# define SUBMENUCOLOR     (4 | A_BOLD)
# define SUBMENUREVCOLOR  (5 | A_BOLD | A_REVERSE)
# define BODYCOLOR        6
# define STATUSCOLOR      (7 | A_BOLD)
# define INPUTBOXCOLOR    8
# define EDITBOXCOLOR     (9 | A_BOLD | A_REVERSE)
# define CURRCOLOR        (10 | A_BOLD)
# define CURRREVCOLOR     (11 | A_BOLD)
# define MARKCOLOR        (12 | A_BOLD)
# define FSTRCOLOR        (13 | A_BOLD)
#else
# define TITLECOLOR       0       /* color pair indices */
# define MAINMENUCOLOR    (A_BOLD)
# define MAINMENUREVCOLOR (A_BOLD | A_REVERSE)
# define SUBMENUCOLOR     (A_BOLD)
# define SUBMENUREVCOLOR  (A_BOLD | A_REVERSE)
# define BODYCOLOR        0
# define STATUSCOLOR      (A_BOLD)
# define INPUTBOXCOLOR    0
# define EDITBOXCOLOR     (A_BOLD | A_REVERSE)
# define CURRCOLOR        (A_BOLD)
# define CURRREVCOLOR     (A_BOLD)
# define MARKCOLOR        (A_BOLD)
# define FSTRCOLOR        (A_BOLD)
#endif


#define th 1     /* title window height */
#define mh 1     /* main menu height */
#define sh 2     /* status window height */
#define bh (LINES - th - mh - sh)   /* body window height */
#define bw COLS  /* body window width */


/******************************* STATIC ************************************/

static WINDOW *wtitl, *wmain, *wbody, *wstat; /* title, menu, body, status win*/
static int nexty, nextx;
static int key = ERR, ch = ERR;
static bool quit = FALSE;
static bool incurses = FALSE;

#ifndef PDCURSES
static char wordchar(void)
{
    return 0x17;    /* ^W */ 
}
#endif

static char *padstr(char *s, int length)
{
    static char buf[MAXSTRLEN+1];
    char fmt[10];

    sprintf(fmt, (int)strlen(s) > length ? "%%.%ds" : "%%-%ds", length);
    sprintf(buf, fmt, s);

    return buf;
}

static char *prepad(char *s, int length)
{
    int i;
    char *p = s;

    if (length > 0)
    {
        memmove((void *)(s + length), (const void *)s, strlen(s) + 1);

        for (i = 0; i < length; i++)
            *p++ = ' ';
    }

    return s;
}

static void rmline(WINDOW *win, int nr)   /* keeps box lines intact */
{
    mvwaddstr(win, nr, 1, padstr(" ", bw - 2));
    wrefresh(win);
}

static void initcolor(void)
{
#ifdef A_COLOR
    if (has_colors())
        start_color();

    /* foreground, background */

    init_pair(TITLECOLOR       & ~A_ATTR, COLOR_BLACK, COLOR_CYAN);      
    init_pair(MAINMENUCOLOR    & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);    
    init_pair(MAINMENUREVCOLOR & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
    init_pair(SUBMENUCOLOR     & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);    
    init_pair(SUBMENUREVCOLOR  & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);   
    init_pair(BODYCOLOR        & ~A_ATTR, COLOR_WHITE, COLOR_BLUE);      
    init_pair(STATUSCOLOR      & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);   
    init_pair(INPUTBOXCOLOR    & ~A_ATTR, COLOR_BLACK, COLOR_CYAN);
    init_pair(EDITBOXCOLOR     & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
    init_pair(CURRCOLOR        & ~A_ATTR, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(CURRREVCOLOR     & ~A_ATTR, COLOR_YELLOW, COLOR_MAGENTA);
    init_pair(MARKCOLOR        & ~A_ATTR, COLOR_GREEN, COLOR_BLUE);
    init_pair(FSTRCOLOR        & ~A_ATTR, COLOR_YELLOW, COLOR_CYAN);
#endif
}

static void setcolor(WINDOW *win, chtype color)
{
    chtype attr = color & A_ATTR;  /* extract Bold, Reverse, Blink bits */

#ifdef A_COLOR
    attr &= ~A_REVERSE;  /* ignore reverse, use colors instead! */
    wattrset(win, COLOR_PAIR(color & A_CHARTEXT) | attr);
#else
    attr &= ~A_BOLD;     /* ignore bold, gives messy display on HP-UX */
    wattrset(win, attr);
#endif
}

static void colorbox(WINDOW *win, chtype color, int hasbox)
{
    int maxy;
#ifndef PDCURSES
    int maxx;
#endif
    chtype attr = color & A_ATTR;  /* extract Bold, Reverse, Blink bits */

    setcolor(win, color);

#ifdef A_COLOR
    if (has_colors())
        wbkgd(win, COLOR_PAIR(color & A_CHARTEXT) | (attr & ~A_REVERSE));
    else
#endif
        wbkgd(win, attr);

    werase(win); 

#ifdef PDCURSES
    maxy = getmaxy(win);
#else
    getmaxyx(win, maxy, maxx);
#endif
    if (hasbox && (maxy > 2))
        box(win, 0, 0);

    touchwin(win);
    wrefresh(win);
}

static void idle(void)
{
    char buf[MAXSTRLEN];
    time_t t;
    struct tm *tp;

    if (time (&t) == -1)
        return;  /* time not available */

    tp = localtime(&t);
    sprintf(buf, " %.2d-%.2d-%.4d  %.2d:%.2d:%.2d",
            tp->tm_mday, tp->tm_mon + 1, tp->tm_year + 1900,
            tp->tm_hour, tp->tm_min, tp->tm_sec);

    mvwaddstr(wtitl, 0, bw - strlen(buf) - 2, buf);
    wrefresh(wtitl); 
}

static void menudim(menu *mp, int *lines, int *columns)
{
    int n, l, mmax = 0;

    for (n=0; mp->func; n++, mp++)
        if ((l = strlen(mp->name)) > mmax) mmax = l;

    *lines = n;
    *columns = mmax + 2;
}

static void setmenupos(int y, int x)
{
    nexty = y;
    nextx = x;
}

static void getmenupos(int *y, int *x)
{
    *y = nexty;
    *x = nextx;
}

static int hotkey(const char *s)
{
    int c0 = *s;    /* if no upper case found, return first char */

    for (; *s; s++)
        if (isupper((unsigned char)*s))
            break;

    return *s ? *s : c0;
}

static void repaintmenu(WINDOW *wmenu, menu *mp)
{
    int i;
    menu *p = mp;
    char *c = NULL;

    for (i = 0; p->func; i++, p++)
    {
        if (ro)
        {
            if ((c = strchr(DISABLEDHOT, (char)(p->name[0]))) != NULL)
                setcolor(wmenu, INPUTBOXCOLOR);
            else
                setcolor(wmenu, SUBMENUCOLOR);
        }
        mvwaddstr(wmenu, i + 1, 2, p->name);
    }

    touchwin(wmenu);
    wrefresh(wmenu);
}

static void repaintmainmenu(int width, menu *mp)
{
    int i;
    menu *p = mp;

    for (i = 0; p->func; i++, p++)
        mvwaddstr(wmain, 0, i * width, prepad(padstr(p->name, width - 1), 1));

    touchwin(wmain);
    wrefresh(wmain);
}

static void mainhelp(void)
{
/*#ifdef ALT_X*/
/*    statusmsg("Use arrow keys and Enter to select (Alt-X to quit)");*/
/*#else*/
    statusmsg("Use arrow keys and Enter to select");
/*#endif*/
}

static void mainmenu(menu *mp)
{
    int nitems, barlen, old = -1, cur = 0, c, cur0;

    menudim(mp, &nitems, &barlen);
    repaintmainmenu(barlen, mp);

    while (!quit)
    {
        if (cur != old)
        {
            if (old != -1)
            {
                mvwaddstr(wmain, 0, old * barlen, 
                          prepad(padstr(mp[old].name, barlen - 1), 1));

                statusmsg(mp[cur].desc);
            }
            else
                mainhelp();

            setcolor(wmain, MAINMENUREVCOLOR);

            mvwaddstr(wmain, 0, cur * barlen, 
                      prepad(padstr(mp[cur].name, barlen - 1), 1));

            setcolor(wmain, MAINMENUCOLOR);
            old = cur;
            wrefresh(wmain);
        }

        switch (c = (key != ERR ? key : waitforkey()))
        {
        case KEY_DOWN:
        case '\n':              /* menu item selected */
            touchwin(wbody);
            wrefresh(wbody);
            rmerror();
            setmenupos(th + mh, cur * barlen);
            curs_set(1);
            (mp[cur].func)();   /* perform function */
            curs_set(0);

            switch (key)
            {
            case KEY_LEFT:
                cur = (cur + nitems - 1) % nitems;
                key = '\n';
                break;

            case KEY_RIGHT:
                cur = (cur + 1) % nitems;
                key = '\n';
                break;

            default:
                key = ERR;
            }

            repaintmainmenu(barlen, mp);
            old = -1;
            break;

        case KEY_LEFT:
            cur = (cur + nitems - 1) % nitems;
            break;

        case KEY_RIGHT:
            cur = (cur + 1) % nitems;
            break;

        case KEY_ESC:
            mainhelp();
            break;

#ifdef PDCURSES
        case KEY_MOUSE:
            getmouse();
                button = 0;
            request_mouse_pos();
            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;
            if (((BUTTON_STATUS(button) & BUTTON_ACTION_MASK) == BUTTON_PRESSED)
            || ((BUTTON_STATUS(button) & BUTTON_ACTION_MASK) == BUTTON_CLICKED))
            {
                if ((MOUSE_Y_POS == 1)
                &&  (MOUSE_X_POS < (nitems*barlen)))
                {
                    if (cur == (int)((MOUSE_X_POS/barlen) % nitems))
                    {
                        setmenupos(th + mh, cur * barlen);
                        curs_set(1);
                        (mp[cur].func)();
                        curs_set(0);
                        repaintmainmenu(barlen, mp);
                        old = -1;
                    }
                    else
                        cur = (int)((MOUSE_X_POS / barlen) % nitems);
                }
                switch (key)
                {
                case KEY_LEFT:
                    cur = (cur + nitems - 1) % nitems;
                    key = '\n';
                    break;
                case KEY_RIGHT:
                    cur = (cur + 1) % nitems;
                    key = '\n';
                    break;
                default:
                    key = ERR;
                }
            }
            break;
#endif

        default:
            cur0 = cur;

            do
            {
                cur = (cur + 1) % nitems;

            } while ((cur != cur0) && (hotkey(mp[cur].name) != toupper(c)));

            if (hotkey(mp[cur].name) == toupper(c))
                key = '\n';
        }

    }

    rmerror();
    touchwin(wbody);
    wrefresh(wbody);
}

static void cleanup(void)   /* cleanup curses settings */
{
    if (incurses)
    {
        delwin(wtitl);
        delwin(wmain);
        delwin(wbody);
        delwin(wstat);
        curs_set(1);
        endwin();
        incurses = FALSE;
#ifdef PDCURSES
        mouse_set(0L);
        PDC_save_key_modifiers(FALSE);
        PDC_return_key_modifiers(FALSE);
#endif
    }
}


/******************************* EXTERNAL **********************************/

void clsbody(void)
{
    werase(wbody);
    wmove(wbody, 0, 0);
}

int bodylen(void)
{
#ifdef PDCURSES
    return getmaxy(wbody);
#else
    int maxy, maxx;

    getmaxyx(wbody, maxy, maxx);
    return maxy;
#endif
}

WINDOW *bodywin(void)
{
    return wbody;
}

void rmerror(void)
{
    rmline(wstat, 0);
}

void rmstatus(void)
{
    rmline(wstat, 1);
}

void titlemsg(char *msg)
{
    if (cry)
       mvwaddstr(wtitl, 0, 1, "C+");
    else if (ro)
       mvwaddstr(wtitl, 0, 1, "R+");
    else if (wr)
       mvwaddstr(wtitl, 0, 1, "W+");
    else
       mvwaddstr(wtitl, 0, 1, "  ");
    mvwaddstr(wtitl, 0, 4, padstr(msg, bw - 5));
    wrefresh(wtitl);
}

void bodymsg(char *msg)
{
    waddstr(wbody, msg);
    wrefresh(wbody);
}

void errormsg(char *msg)
{
    beep();
    mvwaddstr(wstat, 0, 2, padstr(msg, bw - 3));
    wrefresh(wstat);
}

void statusmsg(char *msg)
{
    mvwaddstr(wstat, 1, 2, padstr(msg, bw - 3));
    wrefresh(wstat);
}

bool keypressed(void)
{
    ch = wgetch(wbody);

    return ch != ERR;
}

int getkey(void)
{
    int c = ch;

    ch = ERR;
/*
//#ifdef ALT_X
//    quit = (c == ALT_X);    // PC only !
//#endif
*/
    return c;
}

int waitforkey(void)
{
    if (ontop || bottom || pfind)
    {
        unkeypos++;
        if (ontop && (unkeypos > 2))
            ontop = FALSE;
        else if (pfind && (unkeypos > 3))
            pfind = FALSE;
        else if (bottom && (unkeypos > 5))
            bottom = FALSE;
        else
            return(unkeys[unkeypos]);
    }
    do idle(); while (!keypressed());
    return getkey();
}

void DoExit(void)   /* terminate program */
{
    quit = TRUE;
}

void domenu(menu *mp)
{
    int y, x, nitems, barlen, mheight, mw, old = -1, cur = 0, cur0;
    bool stop = FALSE;
    WINDOW *wmenu;
    char *c = NULL;

    curs_set(0);
    getmenupos(&y, &x);
    menudim(mp, &nitems, &barlen);
    mheight = nitems + 2;
    mw = barlen + 2;
    wmenu = newwin(mheight, mw, y, x);
    colorbox(wmenu, SUBMENUCOLOR, 1);
    repaintmenu(wmenu, mp);

    key = ERR;

    while (!stop && !quit)
    {
        if (cur != old)
        {
            if (old != -1)
            {
                if (ro)
                {
                    if ((c = strchr(DISABLEDHOT, (char)(mp[old].name[0]))) != NULL)
                        setcolor(wmenu, INPUTBOXCOLOR);
                    else
                        setcolor(wmenu, SUBMENUCOLOR);
                }
                mvwaddstr(wmenu, old + 1, 1, 
                          prepad(padstr(mp[old].name, barlen - 1), 1));
            }

            setcolor(wmenu, SUBMENUREVCOLOR);
            mvwaddstr(wmenu, cur + 1, 1,
                      prepad(padstr(mp[cur].name, barlen - 1), 1));

            setcolor(wmenu, SUBMENUCOLOR);
            statusmsg(mp[cur].desc);

            old = cur;
            wrefresh(wmenu);
        }

        switch (key = ((key != ERR) ? key : waitforkey()))
        {
        case '\n':          /* menu item selected */
            touchwin(wbody);
            wrefresh(wbody);
            setmenupos(y + 1, x + 1);
            rmerror();

            key = ERR;
            curs_set(1);
            (mp[cur].func)();   /* perform function */
            curs_set(0);

            repaintmenu(wmenu, mp);

            old = -1;
            break;

        case KEY_UP:
            cur = (cur + nitems - 1) % nitems;
            key = ERR;
            break;

        case KEY_DOWN:
            cur = (cur + 1) % nitems;
            key = ERR;
            break;

        case KEY_ESC:
        case KEY_LEFT:
        case KEY_RIGHT:
            if (key == KEY_ESC)
                key = ERR;  /* return to prev submenu */

            stop = TRUE;
            break;

        case KEY_HOME:
        case KEY_PPAGE:
            cur = 0;
            key = ERR;
            break;
        case KEY_END:
        case KEY_NPAGE:
            cur = nitems-1;
            key = ERR;
            break;

#ifdef PDCURSES
        case KEY_MOUSE:
            getmouse();
                button = 0;
            request_mouse_pos();
            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;
            key = ERR;
            if (((BUTTON_STATUS(button) & BUTTON_ACTION_MASK) == BUTTON_PRESSED)
            || ((BUTTON_STATUS(button) & BUTTON_ACTION_MASK) == BUTTON_CLICKED))
            {
                if ((MOUSE_Y_POS>y) && (MOUSE_Y_POS<=(y+nitems))
                &&  (MOUSE_X_POS>x) && (MOUSE_X_POS<=(x+barlen)))
                {
                    if (cur == (MOUSE_Y_POS-y-1))
                    {
                        setmenupos(y + 1, x + 1);
                        curs_set(1);
                        (mp[cur].func)();
                        curs_set(0);
                        repaintmenu(wmenu, mp);
                        old = -1;
                    }
                    else
                        cur = MOUSE_Y_POS - y -1;
                }
                else
                {
                    if (MOUSE_Y_POS==1)
                    {
                        stop = TRUE;
                        if (MOUSE_X_POS < x)
                           key = KEY_LEFT;
                        else if (MOUSE_X_POS > (x+barlen))
                           key = KEY_RIGHT;
                    }
                }
            }
            break;
#endif

        default:
            cur0 = cur;

            do
            {
                cur = (cur + 1) % nitems;

            } while ((cur != cur0) &&
                     (hotkey(mp[cur].name) != toupper((int)key)));

            key = (hotkey(mp[cur].name) == toupper((int)key)) ? '\n' : ERR;
        }

    }

    rmerror();
    delwin(wmenu);
    touchwin(wbody);
    wrefresh(wbody);
}

void init()
{
    initscr();
    incurses = TRUE;
    initcolor();

    wtitl = subwin(stdscr, th, bw, 0, 0);
    wmain = subwin(stdscr, mh, bw, th, 0);
    wbody = subwin(stdscr, bh, bw, th + mh, 0);
    wstat = subwin(stdscr, sh, bw, th + mh + bh, 0);

    colorbox(wtitl, TITLECOLOR, 0);
    colorbox(wmain, MAINMENUCOLOR, 0);
    colorbox(wbody, BODYCOLOR, 0);
    colorbox(wstat, STATUSCOLOR, 0);

    cbreak();              /* direct input (no newline required)... */
    noecho();              /* ... without echoing */
    curs_set(0);           /* hide cursor (if possible) */
    nodelay(wbody, TRUE);  /* don't wait for input... */
    halfdelay(10);         /* ...well, no more than a second, anyway */
    keypad(wbody, TRUE);   /* enable cursor keys */
    scrollok(wbody, TRUE); /* enable scrolling in main window */

    leaveok(stdscr, TRUE);
    leaveok(wtitl, TRUE);
    leaveok(wmain, TRUE);
    leaveok(wstat, TRUE);
#ifdef PDCURSES
    mouse_set(ALL_MOUSE_EVENTS);
    PDC_save_key_modifiers(TRUE);
    PDC_return_key_modifiers(TRUE);
#endif
    newterm(getenv("TERM"), stderr, stdin);
    keypad(stdscr,  TRUE);
}

void startmenu(menu *mp, char *mtitle)
{
    if (mtitle)
        titlemsg(mtitle);

    mainmenu(mp);

    cleanup();
}

static void repainteditbox(WINDOW *win, int x, char *buf)
{
#ifndef PDCURSES
    int maxy;
#endif
    int maxx;

#ifdef PDCURSES
    maxx = getmaxx(win);
#else
    getmaxyx(win, maxy, maxx);
#endif
    werase(win);
    mvwprintw(win, 0, 0, "%s", padstr(buf, maxx));
    wmove(win, 0, x);
    wrefresh(win); 
}

/*

  weditstr()     - edit string

  Description:
    The initial value of 'str' with a maximum length of 'field' - 1,
    which is supplied by the calling routine, is editted. The user's 
    erase (^H), kill (^U) and delete word (^W) chars are interpreted. 
    The PC insert or Tab keys toggle between insert and edit mode.
    Escape aborts the edit session, leaving 'str' unchanged.
    Enter, Up or Down Arrow are used to accept the changes to 'str'.
    NOTE: editstr(), mveditstr(), and mvweditstr() are macros.

  Return Value:
    Returns the input terminating character on success (Escape, 
    Enter, Up or Down Arrow) and ERR on error.

  Errors:
    It is an error to call this function with a NULL window pointer.
    The length of the initial 'str' must not exceed 'field' - 1.

*/

int weditstr(WINDOW *win, char *buf, int field)
{
    char org[MAXSTRLEN], *tp, *bp = buf;
    bool defdisp = TRUE, stop = FALSE, insert = FALSE;
    int cury, curx, begy, begx, oldattr;
    WINDOW *wedit;
    int c = 0;
    int i, j;
#if USE_WIDECH
    wint_t ch;
#endif

    if ((field >= MAXSTRLEN) || (buf == NULL) ||
        ((int)strlen(buf) > field - 1))
        return ERR;

    strcpy(org, buf);   /* save original */

    c = strlen(buf)-1;
    while ((c>0) && (buf[c]==' '))
    {
        buf[c] = '\0';
        c--;
    }

    wrefresh(win);
    getyx(win, cury, curx);
    getbegyx(win, begy, begx);

    wedit = subwin(win, 1, field, begy + cury, begx + curx);
    oldattr = wedit->_attrs;
    colorbox(wedit, EDITBOXCOLOR, 0);

    keypad(wedit, TRUE);
    curs_set(1);

    while (!stop)
    {
        idle();
        repainteditbox(wedit, bp - buf, buf);

        switch (c = wgetch(wedit))
        {
        case ERR:
            break;

        case KEY_ESC:
            strcpy(buf, org);   /* restore original */
            stop = TRUE;
            break;

        case '\n':
        case KEY_UP:
        case KEY_DOWN:
        case '\t':
            stop = TRUE;
            break;

        case KEY_LEFT:
            if (bp > buf)
                bp--;
            break;

        case KEY_RIGHT:
            defdisp = FALSE;
            if (bp - buf < (int)strlen(buf))
                bp++;
            break;

      /*case '\t':*/          /* TAB -- because insert
                                  is broken on HPUX */
        case KEY_IC:          /* enter insert mode */
        case KEY_EIC:         /* exit insert mode */
            defdisp = FALSE;
            insert = !insert;

            curs_set(insert ? 2 : 1);
            break;

        case KEY_DC:
            strcpy(bp, bp+1);
            break;
        case KEY_HOME:
            bp = buf;
            break;
        case KEY_END:
            defdisp = FALSE;
            bp = (buf+strlen(buf));
            break;
        case CTL_LEFT:
            while ((bp > buf) && (*(bp - 1) == ' ')) 
                bp--;
            while ((bp > buf) && (*(bp - 1) != ' ')) 
                bp--;
            break;
        case CTL_RIGHT:
            while ((bp - buf < (int)strlen(buf)) && (*(bp) != ' ')) 
                bp++;
            while ((bp - buf < (int)strlen(buf)) && (*(bp) == ' ')) 
                bp++;
            break;
        case CTL_END:
            *bp = '\0';
            break;
#ifdef PDCURSES
        case KEY_MOUSE:
            getmouse();
                button = 0;
            request_mouse_pos();
            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;
            key = ERR;
            if (((BUTTON_STATUS(button) & BUTTON_ACTION_MASK) == BUTTON_PRESSED)
            || ((BUTTON_STATUS(button) & BUTTON_ACTION_MASK) == BUTTON_CLICKED))
            {
                if ((MOUSE_Y_POS == (begy+cury))
                &&  (MOUSE_X_POS >= (begx+curx))
                && (MOUSE_X_POS < (begx+curx+field)))
                {
                    bp = buf + (MOUSE_X_POS-(begx+curx));
                    if (bp - buf > (int)strlen(buf))
                       bp = (buf+strlen(buf));
                }
                else
                {
                    if ((MOUSE_Y_POS >= begy)
                    && (MOUSE_Y_POS <= (begy+cols+2))
                    && (MOUSE_X_POS >= begx)
                    && (MOUSE_X_POS <= (begx+curx+field)))
                         c = (MOUSE_Y_POS < (begy+cury)) ? KEY_UP : KEY_DOWN;
                    else
                         c = KEY_ESC;
                    stop = TRUE;
                }
            }
            break;
#endif
        case CTRL_C:
            i = 0;
            tp = buf;
            while (tp[0] != '\0')
            {
                clp[i] = *tp++;
                i++;
            }
            break;
        case CTRL_V:
            j = strlen(clp);
            if (((bp+j) - buf) < (field - 1))
            {
                if (insert)
                {
                    memmove((void *)(bp + j), (const void *)bp,
                            strlen(bp) + j);
                }
                for (i=0; i<j; i++)
                {
                    *bp++ = clp[i];
                }
            }
            defdisp = FALSE;
            break;
        case CTRL_B:
            j = strlen(fstr);
            if (((bp+j) - buf) < (field - 1))
            {
                if (insert)
                {
                    memmove((void *)(bp + j), (const void *)bp,
                            strlen(bp) + j);
                }
                for (i=0; i<j; i++)
                {
                    *bp++ = fstr[i];
                }
            }
            defdisp = FALSE;
            break;
        case CTRL_D:
            j = strlen(buf);
            for (i=0; i<j; i++)
                if (!isdigit(buf[i]))
                    j = 0;
            if (j == 8)
            {
                buf[10] = '\0';
                buf[9] = (char)buf[7];
                buf[8] = (char)buf[6];
                buf[7] = (char)'.';
                buf[6] = (char)buf[5];
                buf[5] = (char)buf[4];
                buf[4] = (char)'.';
            }
            defdisp = FALSE;
            break;
        case CTRL_U:
            casestr(buf, TRUE, FALSE);
            break;
        case CTRL_L:
            casestr(buf, FALSE, FALSE);
            break;
        case ALT_U:
            capfstr(buf, TRUE, FALSE);
            break;
        case ALT_L:
            capfstr(buf, FALSE, FALSE);
            break;
        case KEY_F(1):
            if (regexp == TRUE)
               REGHLP;
            else
               EDITHLP;
            touchwin(win);
            wrefresh(win);
            wrefresh(wedit);
            break;
/*#ifdef DJGPP*/
        case CTRL_X:
            i = 0;
            tp = buf;
            while (tp[i] != '\0')
            {
                switch (tp[i])
                {
                case '\'':
                    tp[i] = 0xA0;
                    break;
                case '"':
                    tp[i] = 0xB5;
                    break;
                case ';':
                    tp[i] = 0x82;
                    break;
                case ':':
                    tp[i] = 0x90;
                    break;
                case '\\':
                    tp[i] = 0xA1;
                    break;
                case '|':
                    tp[i] = 0x92;
                    break;
                case 'ˇ': /*161*/
                    tp[i] = 0xFB;
                    break;
                case '’': /*146*/
                    tp[i] = 0xEB;
                    break;
                case 'ű': /*251*/
                    tp[i] = 0xA1;
                    break;
                case 'ë': /*235*/
                    tp[i] = 0x92;
                    break;
                case '=':
                    tp[i] = 0xA2;
                    break;
                case '+':
                    tp[i] = 0xE0;
                    break;
                case '[':
                    tp[i] = 0x8B;
                    break;
                case '{':
                    tp[i] = 0x8A;
                    break;
                case '0':
                    tp[i] = 0x94;
                    break;
                case '”': /*148*/
                    tp[i] = 0x30;
                    break;
                case ')':
                    tp[i] = 0x99;
                    break;
                case ']':
                    tp[i] = 0xA3;
                    break;
                case '}':
                    tp[i] = 0xE9;
                    break;
                case '-':
                    tp[i] = 0x81;
                    break;
                case '_':
                    tp[i] = 0x9A;
                    break;
                }
                i++;
            }
            break;
/*#endif //DJGPP*/
        default:
            if (c == erasechar())       /* backspace, ^H */
            {
                if (bp > buf)
                {
                    memmove((void *)(bp - 1), (const void *)bp, strlen(bp) + 1);
                    bp--;
                }
            }
            else if (c == killchar())   /* ^U */
            {
                bp = buf;
                *bp = '\0';
            }
            else if (c == wordchar())   /* ^W */
            {
                tp = bp;

                while ((bp > buf) && (*(bp - 1) == ' ')) 
                    bp--;
                while ((bp > buf) && (*(bp - 1) != ' ')) 
                    bp--;

                memmove((void *)bp, (const void *)tp, strlen(tp) + 1);
            }
            else if ((c > 0x1FB)) /* 507 */
            {
                c = 0;
            }
/*            else if (isprint(c))*/
            else if (!iscntrl(c)
                     || (c==0x81 || c==0x90))
            {
                if (defdisp)
                {
                    bp = buf;
                    *bp = '\0';
                    defdisp = FALSE;
                }

                if (insert)
                {
                    if ((int)strlen(buf) < field - 1)
                    {
                        memmove((void *)(bp + 1), (const void *)bp,
                                strlen(bp) + 1);

                        *bp++ = c;
                    }
                }
                else if (bp - buf < field - 1)
                {
                    /* append new string terminator */
                    if (!*bp)
                        bp[1] = '\0';
                    *bp++ = c;
                }
            }
        }
    }

    curs_set(0);

    wattrset(wedit, oldattr);
    repainteditbox(wedit, bp - buf, buf);
    delwin(wedit);

    return c;
}

WINDOW *winputbox(WINDOW *win, int nlines, int ncols)
{
    WINDOW *winp;
    int cury, curx, begy, begx;

    getyx(win, cury, curx);
    getbegyx(win, begy, begx);

    winp = newwin(nlines, ncols, begy + cury, begx + curx);
    colorbox(winp, INPUTBOXCOLOR, 1);

    return winp;
}

int getstrings(char *desc[], char *buf[], int field, int length)
{
    WINDOW *winput;
    int oldy, oldx, maxy, maxx, nlines, ncols, i, n, l, mmax = 0;
    int c = 0;
    bool stop = FALSE;

    for (n = 0; desc[n]; n++)
        if ((l = strlen(desc[n])) > mmax)
            mmax = l;

    nlines = n + 2; ncols = mmax + length + 4;
    getyx(wbody, oldy, oldx);
    getmaxyx(wbody, maxy, maxx);

    winput = mvwinputbox(wbody, (maxy - nlines) / 2, (maxx - ncols) / 2, 
        nlines, ncols);

    for (i = 0; i < n; i++)
        mvwprintw(winput, i + 1, 2, "%s", desc[i]);

    i = 0;

    while (!stop)
    {
        l = (field == 0) ? length : len[i]+1;
        switch (c = mvweditstr(winput, i+1, mmax+3, buf[i], l))
        {
        case KEY_ESC:
            stop = TRUE;
            break;

        case KEY_UP:
            i = (i + n - 1) % n;
            break;

        case '\n':
        case '\t':
        case KEY_DOWN:
            if (++i == n)
                stop = TRUE;    /* all passed? */
        }
    }

    delwin(winput);
    touchwin(wbody);
    wmove(wbody, oldy, oldx);
    wrefresh(wbody);

    return c;
}


/**************************** string entry box ****************************/

char *getfname(char *desc, char *fname, int length)
{
    char *fieldname[2];
    char *fieldbuf[2];

    fieldname[0] = desc;
    fieldname[1] = 0;
    fieldbuf[0] = fname;
    fieldbuf[1] = 0;

    return (getstrings(fieldname, fieldbuf, 0, length) == KEY_ESC) ? NULL : fname;
}


/****DAT****/

int casestr(char *str, bool upper, bool ascii)
{
#define MAXCHS 9
  char chr_lo[] = " ‚ˇ˘”‹Łű";
  char chr_hi[] = "µÖŕ™Šéšë";
  char asc_lo[] = "aeiooouuu";
  char asc_hi[] = "AEIOOOUUU";
/*  char chr_utflo[] = "ĂˇĂ©Ă­ĂłĂ¶Ĺ‘ĂşĂĽĹ±";*/
/*  char chr_utfhi[] = "ĂĂ‰ĂŤĂ“Ă–ĹĂšĂśĹ°";*/
  int i, j;
  int len=strlen(str);
  unsigned char c;

  for (i=0; i<len; i++)
  {
      if ((c=str[i]) < 0x7F)
          str[i] = (upper ? toupper(c) : tolower(c));
      else
      {
         for (j=0; j<MAXCHS; j++)
         {
             if (upper)
             {
                if (c == (unsigned char)chr_lo[j])
                {
                   str[i] = (ascii ? asc_hi[j] : chr_hi[j]);
                   j = MAXCHS;
                }
                else if (ascii && (c == (unsigned char)chr_hi[j]))
                     {
                        str[i] = asc_hi[j];
                        j = MAXCHS;
                     }
             }
             else
             {
                if (c == (unsigned char)chr_hi[j])
                {
                   str[i] = (ascii ? asc_lo[j] : chr_lo[j]);
                   j = MAXCHS;
                }
                else if (ascii && (c == (unsigned char)chr_lo[j]))
                     {
                        str[i] = asc_lo[j];
                        j = MAXCHS;
                     }
             }
         }
      }
  }
  return 0;
}

int capfstr(char *str, bool upper, bool ascii)
{
  int i, j;
  bool change;
  char s[] = "?";

  change = (str[0] == 0x20) ? FALSE : TRUE;
  j = strlen(str);
  for (i=0; i<j; i++)
  {
      if (change)
      {
          s[0] = str[i];
          casestr(s, upper, ascii);
          str[i] = s[0];
      }
      change = (str[i] == 0x20) ? TRUE : FALSE;
  }
  return 0;
}

int hstrcmp(const char *s1, const char *s2)
{
    char e1[MAXSTRLEN];
    char e2[MAXSTRLEN];

    strcpy(e1, s1);
    strcpy(e2, s2);
    casestr(e1, TRUE, TRUE);
    casestr(e2, TRUE, TRUE);
    return strcmp(e1, e2);
}

int qsort_stringlist(const void *e1, const void *e2)
{
    if (sortpos == 0)
        return hstrcmp(*(char **)e1, *(char **)e2);
    else
        return hstrcmp(*(char **)(e1)+sortpos, *(char **)(e2)+sortpos);
}

void sort(int n)
{
    qsort(rows, n, sizeof(char *), qsort_stringlist);
}

int qs_stringlist_rev(const void *e1, const void *e2)
{
    if (sortpos == 0)
        return hstrcmp(*(char **)e2, *(char **)e1);
    else
        return hstrcmp(*(char **)(e2)+sortpos, *(char **)(e1)+sortpos);
}

void sort_back(int n)
{
    qsort(rows, n, sizeof(char *), qs_stringlist_rev);
}

void flagmsg(void)
{
    setcolor(wtitl, modified ? FSTRCOLOR : TITLECOLOR);
    mvwaddstr(wtitl, 0, 0, (modified ? "*" : " "));
    wrefresh(wtitl);
}

int bodywidth(void)
{
#ifdef PDCURSES
    return getmaxx(wbody);
#else
    int maxy, maxx;

    getmaxyx(wbody, maxy, maxx);
    return maxx;
#endif
}

void displn(int y, int r)
{
    int i, j, k;
    int maxlen;
    char s[MAXCOLS][MAXSTRLEN+1];
    char buf[MAXSTRLEN+1];
    char *p=NULL;

    maxlen = bodywidth()-2;
    i = 0;
    strcpy(buf, rows[y]+beg[curcol]);
    p = strtok(buf, ssep );
    for (i=0; i<=cols; i++)
    {
        if (p != NULL)
        {
            strcpy(s[i], p);
            k = strlen(s[i]);
            for (j=0; j<(len[i]); j++)
            {
               if ((s[i][j] == csep)
               || (s[i][j] == '\n'))
                  k = j;
               if (j >= k)
                  s[i][j] = ' ';
            }
            s[i][len[i]] = '\0';
            p = strtok( 0, ssep );
        }
        else
        {
            strcpy(s[i], " ");
            for (k=1; k<len[i]; k++)
               strcat(s[i], " ");
        }
        if (y==curr)
        {
           if (i==field)
              setcolor(wbody, CURRREVCOLOR);
           else
              setcolor(wbody, CURRCOLOR);
        }
        else
           setcolor(wbody, BODYCOLOR);
        if (beg[i+1] <= maxlen)
        {
           mvwaddstr(wbody, r, beg[i], s[i]);
           mvwaddstr(wbody, 0, beg[i], stru[i]);
        }
    }
    if (beg[field+1] > maxlen)
    {
        setcolor(wbody, MARKCOLOR);
        mvwaddstr(wbody, r, maxlen, ">");
    }
    else
        mvwaddstr(wbody, r, beg[field], "");
    if (y == reccnt)
        wclrtoeol(wbody);
    if (flags[y] == 1)
    {
        setcolor(wbody, MARKCOLOR);
        mvwaddstr(wbody, r, MIN(strlen(head), maxlen)-1, "@");
    }
    else
    {
        setcolor(wbody, BODYCOLOR);
        mvwaddstr(wbody, r, MIN(strlen(head), maxlen)-1, " ");
    }
    setcolor(wbody, BODYCOLOR);
    wclrtoeol(wbody);
    wrefresh(wbody);
}

void statusln(void)
{
    char buf[MAXSTRLEN];

    sprintf(buf, "%5u/%u (%u/%u) ", curr+1, reccnt, field+1, cols+1);
    setcolor(wbody, STATUSCOLOR);
    mvwaddstr(wstat, 0, 0, buf);
    mvwaddstr(wstat, 1, 0, ">");
    statusmsg(rows[curr]);
}

void crypt(int n)
{
#define KEYLEN 17
    char key[KEYLEN]="_*tSvDb_EnCrYpT*_";
    char k0, k1, k2;
    int i, j, k, l;

    for (i=0; i<n; i++)
    {
        l = strlen(rows[i]);
        k = 1;
        for (j=0; j<l; j++)
        {
            k1 = rows[i][j];
            if (k1 != 9)        // \t
            {
                if (k1 > 0x1F)
                {
                    k2 = key[k-1] + (j%k)+i;
                    k2 &= 0x1F; //31.
                    k0 = (char) (k1 ^ k2);
                    rows[i][j] = k0;
                }
                if (k < KEYLEN) k++;
                else k=1;
            }
        }
    }
    j = strlen(head);
    if (head[0] == '\3')
    {
        for (i=0; i<j; i++)
            head[i] = head[i+1];
        if (!locked)
            ro = FALSE;
        cry= FALSE;
    }
    else
    {
        for (i=j+1; i>0; i--)
            head[i] = head[i-1];
        head[0] = '\3';
        ro = TRUE;
        cry= TRUE;
    }
    crypted = TRUE;
    curr=0;
    field=0;
    curcol=0;
    clsbody();
    wrefresh(wbody);
    j = 0;
    for (i=0; i<bodylen(); i++)
    {
        if (i < reccnt)
           displn(i, j+1);
        j++;
    }
    titlemsg(datfname);
}

int loadfile(char *fname)
{
    int i, j, k;
    FILE *fp;
    char buf[MAXSTRLEN+1];
    bool ateof = FALSE;
    char *p;

    if (modified == TRUE)
    {
        sprintf(buf, "ERROR: file '%s' not saved", datfname);
        errormsg(buf);
        return -1;
    }

    if (((p = strstr(fname, ".tsv")) == NULL)
    ||  ((p = strstr(fname, ".csv")) == NULL))
    {
        if ((fp = fopen(fname, "r")) == NULL)
        {
            strcat(fname, ".tsv");
        }
        else
            fclose(fp);
    }

    if ((fp = fopen(fname, "r")) != NULL)
    {
        buf[0] = '\0';
        fgets(buf, MAXSTRLEN, fp);
        if (strstr(buf, ssep) == NULL)
        {
            sprintf(buf, "ERROR: Can't open file '%s'", fname);
            errormsg(buf);
            return -1;
        }
        strcpy(head, buf);
        if (head[0] == '\3')
        {
            j = strlen(buf);
            for (i=0; i<j; i++)
                buf[i] = buf[i+1];
            cry = TRUE;
            ro = TRUE;
        }
        else
        {
            cry = FALSE;
        }
        crypted = cry;
        j = strlen(buf);
        while ((buf[j-1] == '\n') || (buf[j-1] == '\n'))
        {
            j--;
            buf[j] = '\0';
        }
        if (j)
        {
            i = 0;
            k = 0;
            p = strtok(buf, ssep );
            while(p != NULL )
            {
                strcpy(stru[i], p);
                j = strlen(stru[i]);
                strcat(stru[i], " ");
                beg[i] = k;
                len[i] = j+1;
                k += j+1;
                i++;
                if (i >= MAXCOLS)
                   break;
                p = strtok(NULL, ssep );
            }
            cols = i-1;
            len[cols]--;
            stru[cols][len[cols]] = '\0';
        }
        //fsetpos(fp, 0);
        i = 0;
        while (!ateof)
        {
            buf[0] = '\0';
            fgets(buf, MAXSTRLEN, fp);
            j = strlen(buf);
            if (j)
            {
                rows[i] = (char *)malloc(j+1);
                strcpy(rows[i], buf);
                i++;
                if (i>MAXROWS)
                    ateof = TRUE;
            }
            else
                ateof = TRUE;
        }
        reccnt = i;
        fclose(fp);
        rows[reccnt] = (char *)malloc(2);
        strcpy(rows[reccnt], "\0");
    }
    else
    {
        sprintf(buf, "ERROR: file '%s' not found", fname);
        errormsg(buf);
        return -1;
    }
    if (locked && cry)
    {
        crypt(reccnt);
    }
    field=0;
    curcol=0;
    clsbody();
    wrefresh(wbody);
    j = 0;
    for (i=0; i<bodylen(); i++)
    {
        if (i < reccnt)
           displn(i, j+1);
        j++;
    }
    return 0;
}


static WINDOW *wmsgmsg;

void msg(char *msg)
{
    int i;

    if (msg != NULL)
    {
        i = strlen(msg)+4;
        wmsgmsg = mvwinputbox(wbody, (bodylen()-2)/2, (bodywidth()-i)/2, 3, i);
        mvwaddstr(wmsgmsg, 1, 2, msg);
        wrefresh(wmsgmsg);
    }
    else
    {
        delwin(wmsgmsg);
        touchwin(wbody);
        wrefresh(wbody);
    }
}

int yesno(char *msg)
{
    WINDOW *wmsg;
    int i;

    i = strlen(msg)+4;
    wmsg = mvwinputbox(wbody, (bodylen()-1)/3, (bodywidth()-i)/2, 3, i);
    mvwaddstr(wmsg, 1, 2, msg);
    wrefresh(wmsg);
    i = toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
    return (i == 'Y' ? 1 : 0);
}

#ifndef S_IREAD
#define _S_IREAD 0x0100
#define S_IREAD _S_IREAD
#endif
#ifndef S_IWRITE
#define _S_IWRITE 0x0080
#define S_IWRITE _S_IWRITE
#endif
#ifndef S_IRUSR
#define	S_IRUSR		_S_IRUSR
#define	_S_IRUSR	_S_IREAD
#endif
#ifndef S_IWUSR
#define	_S_IWUSR	_S_IWRITE
#define	S_IWUSR		_S_IWUSR
#endif

int savefile(char *fname, int force)
{
    int i;
    FILE *fp;
    char buf[MAXSTRLEN+1];

    if (ro && !cry)
        return -1;
    (void)chmod(fname, S_IWUSR);
    if ((force==0) && ((fp = fopen(fname, "r")) != NULL))
    {
        fclose(fp);
        if (strcmp(fname, datfname) != 0)
        {
           if (yesno("File exists! Overwrite it? (Y/N):") == 0)
              return -1;
        }
    }
    if ((fp = fopen(fname, "w")) != NULL)
    {
        msg(WSTR);
        if (locked && !cry)
        {
            crypt(reccnt);
        }
        fputs(head, fp);
        for (i=0; i<reccnt; i++)
            fputs(rows[i], fp);
        fclose(fp);
        msg(NULL);
    }
    else
    {
        sprintf(buf, "ERROR: file '%s' not saved", fname);
        errormsg(buf);
        return -1;
    }
    (void)chmod(fname, S_IRUSR);
    modified = FALSE;
    flagmsg();
    return 0;
}

void clean()
{
    int i;

    for (i=0; i<=reccnt; i++)

    {
        if (rows[i] != NULL)
           free(rows[i]);
    }
    reccnt = 0;
}

int create(char *fn)
{
    char buf[MAXSTRLEN+1];
    int i, j, k;
    char *p;
    bool new=TRUE;

    if (ro)
        return -1;
    if (modified == TRUE)
    {
        sprintf(buf, "ERROR: file '%s' not saved", datfname);
        errormsg(buf);
        return -1;
    }
    strcpy(buf, fn);
    j = strlen(head);
    if ((j == 0) || (j > 66))
       strcpy(buf, "?");
    else
    {
       strcpy(buf, head);
       j = strlen(buf);
       for (i=0; i<j; i++)
          if (buf[i] == csep)
             buf[i] = ' ';
       new = FALSE;
    }
    if (getfname("Head:", buf, 68))
    {
        strcat(buf, "\n");
        j = strlen(buf);
        if (j)
        {
            for (i=0; i<j; i++)
            {
                if (buf[i]==' ')
                   buf[i] = csep;
            }
            strcpy(head, buf);
            strcat(head, "\n");
            i = 0;
            k = 0;
            p = strtok(buf, ssep );
            while(p != NULL )
            {
                strcpy(stru[i], p);
                j = strlen(stru[i]);
                strcat(stru[i], " ");
                beg[i] = k;
                len[i] = j+1;
                k += j+1;
                i++;
                if (i >= MAXCOLS)
                   break;
                p = strtok(NULL, ssep );
            }
            cols = i-1;
            //head[beg[cols]+len[cols]] = '\0';
            len[cols]--;
            stru[cols][len[cols]-1] = ' ';
            stru[cols][len[cols]] = '\0';
            if (new)
            {
                strcpy(datfname, FNAME);
                clean();
                rows[0] = (char *)malloc(2);
                strcpy(rows[0], "\n");
                reccnt = 0;
                rows[reccnt+1] = NULL;
            }
        }
        if (savefile(datfname, 1) == 0)
        {
            titlemsg(datfname);
            modified = TRUE;
            clsbody();
            wrefresh(wbody);
            flagmsg();
        }
        else 
            return -1;
    }
    return 0;
}

void brows(void)
{
    int i;
    int j = bodylen()-1;
    int k = reccnt-bodylen()+1;
    int x;
    int len = MIN(bodywidth(), strlen(head));
    bool firsttab;
    bool quit = FALSE;
    char buf[MAXSTRLEN+1];

    if (reccnt == 0)
       return;
    setcolor(wbody, BODYCOLOR);
    clsbody();
    curr = 0;
    while (!quit)
    {
        wmove(wbody,0,0);
        for (i=curr; i<(curr+j); i++)
        {
            if (i >= reccnt)
               break;
            (void)itoa(i+1, buf, 10);
            x = 9-strlen(buf);
            strcat(buf, ssep);
            strcat(buf, rows[i]);
            x = MIN(strlen(buf), len-x);
            buf[x-1] = '\n';
            buf[x] = '\0';
            firsttab = TRUE;
            for (x=0; x<len; x++)
            {
               if (buf[x] == csep)
               {
                   if (firsttab)
                       firsttab = FALSE;
                   else
                       buf[x] = '|';
               }
            }
            bodymsg(buf);
        }
        switch (waitforkey())
        {
        case KEY_UP:
            if (curr>0)
               curr--;
            break;
        case KEY_DOWN:
            if (curr<k)
               curr++;
            break;
        case KEY_HOME:
            curr = 0;
            break;
        case KEY_END:
            curr = k;
            break;
        case KEY_PPAGE:
            curr -= j;
            if (curr < 0)
               curr = 0;
            break;
        case KEY_NPAGE:
            curr += j;
            if (curr > k)
               curr = k;
            break;
#ifdef PDCURSES
        case KEY_MOUSE:
            getmouse();
                button = 0;
            request_mouse_pos();
            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;
            if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_PRESSED)
            {
                if ((MOUSE_Y_POS == 1) && (MOUSE_X_POS < 18))
                   quit = TRUE;
                else
                {
                   if (MOUSE_Y_POS < (j/2))
                      curr = curr > j ? curr-j : 0;
                   else
                      curr = (curr+j) < k ? curr+j : k;
                }
            }
            break;
#endif
        case 'Q':
        case 'q':
        case 0x1b:
            quit = TRUE;
        }
    }
}

void fltls(void)
{
    int i, j, k;
    int x = reccnt-1;
    int len=bodywidth()-9;
    bool quit = FALSE;
    char buf[MAXSTRLEN+1];

    setcolor(wbody, BODYCOLOR);
    wclear(wstat);
    wrefresh(wstat);
    curr = 0;
    while (!quit)
    {
        clsbody();
        wmove(wbody,0,0);
        i = curr;
        j = bodylen()-1;
        while (i < (curr+j))
        {
            if (flags[i])
            {
               (void)itoa(i+1, buf, 10);
               strcat(buf, ssep);
               strcat(buf, rows[i]);
               if (strlen(buf) > len)
               {
                  buf[len] = '\n';
                  buf[len+1] = '\0';
               }
               for (k=10; k<len; k++)
                  if (buf[k] == csep)
                      buf[k] = '|';
               bodymsg(buf);
            }
            else
            {
               if (i < x)
                  j++;
               else
                  break;
            }
            i++;
        }
        j = bodylen()-1;
        switch (waitforkey())
        {
        case KEY_PPAGE:
        case KEY_UP:
            if (curr > 0)
               curr--;
            while (flags[curr] != 1 && curr > 0)
               curr--;
            break;
        case KEY_NPAGE:
        case KEY_DOWN:
            if (curr < x)
               curr++;
            while (flags[curr] != 1 && curr < x)
               curr++;
            break;
        case KEY_HOME:
            curr = 0;
            break;
        case KEY_END:
            clsbody();
            curr = reccnt-1;
            while (flags[curr] != 1 && curr > 1)
               curr--;
            if (curr > x)
               curr = x;
            break;
#ifdef PDCURSES
        case KEY_MOUSE:
            getmouse();
                button = 0;
            request_mouse_pos();
            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;
            if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_PRESSED)
            {
                if ((MOUSE_Y_POS == 1) && (MOUSE_X_POS < 18))
                   quit = TRUE;
                else
                {
                   if (MOUSE_Y_POS < (j/2))
                      curr = curr > j ? curr-j : 0;
                   else
                      curr = (curr+j) < x ? curr+j : x;
                }
            }
            break;
#endif
        case 'Q':
        case 'q':
        case 0x1b:
            quit = TRUE;
        }
    }
}

void modify(int y)
{
    int i, j, k;
    char s[MAXCOLS][MAXSTRLEN+1];
    char buf[MAXSTRLEN+1];
    char *p=NULL;
    char *fieldnam[MAXCOLS+1];
    char *fieldbuf[MAXCOLS+1];
    int flen = 0;

    if (ro)
        return;
    strcpy(buf, rows[y]);
    p = strtok(buf, ssep );
    for (i=0; i<=cols; i++)
    {
        if (p != NULL)
        {
            strcpy(s[i], p);
            k = strlen(s[i]);
            for (j=0; j<(len[i]); j++)
            {
               if ((s[i][j] == csep)
               ||  (s[i][j] == '\n'))
                  k = j;
               if (j >= k)
                  s[i][j] = ' ';
            }
            s[i][len[i]] = '\0';
            p = strtok( 0, ssep );
        }
        else
        {
            strcpy(s[i], " ");
            for (k=1; k<len[i]; k++)
               strcat(s[i], " ");
        }
    }
    for (i = 0; i <= cols; i++)
    {
        fieldnam[i] = stru[i];
        fieldbuf[i] = s[i];
        flen = (len[i] > flen) ? len[i] : flen;
    }
    flen += 2;
    fieldnam[cols+1] = (char *)0;
    fieldbuf[cols+1] = s[cols+1];
    if (getstrings(fieldnam, fieldbuf, 1, flen) == KEY_ESC)
        return;
    else
    {
        modified = TRUE;
        flagmsg();
    }
    for (i = 0; i <= cols; i++)
    {
        k = len[i];
        fieldbuf[i][k] = '\0';
        k--;
        for (j=k; j>0; j--)
        {
            if (fieldbuf[i][j] == ' ')
                fieldbuf[i][j] = '\0';
            else
                j = 0;
        }
    }
    strcpy(buf, fieldbuf[0]);
    for (i = 1; i <= cols; i++)
    {
        if ((fieldbuf[i] != NULL)
        && (fieldbuf[i][0] != '\0'))
        {
           strcat(buf, ssep);
           strcat(buf, fieldbuf[i]);
        }
    }
    strcat(buf, "\n\0");
    i = strlen(buf);
    if (rows[y] != NULL)
       free(rows[y]);
    rows[y] = (char *)malloc(i+1);
    strcpy(rows[y], buf);
}

void newrec(int y)
{
    int i;

    if (ro)
        return;
    reccnt++;
    rows[reccnt+1] = NULL;
    for (i=reccnt; i>y; i--)
    {
        rows[i] = rows[i-1];
    }
    rows[y] = (char *)malloc(2);
    strcpy(rows[y], "\n");
    modified = TRUE;
    flagmsg();
}

void dupl(int y)
{
    int i;

    if (ro)
        return;
    reccnt++;
    rows[reccnt+1] = NULL;
    for (i=reccnt; i>y; i--)
    {
        rows[i] = rows[i-1];
    }
    i = strlen(rows[y+1]);
    rows[y] = (char *)malloc(i+1);
    strcpy(rows[y], rows[y+1]);
    modified = TRUE;
    flagmsg();
}

void purge(int y)
{
    int i;

    if (ro)
        return;
    if (y<1)
    {
        yesno("First record, edit it!");
        return;
    }
    if ((i=yesno("Delete record! Are You sure? (Y/N):")) == 0)
        return;
    if (rows[y] != NULL)
       free(rows[y]);
    for (i=y; i<reccnt; i++)
    {
        rows[i] = rows[i+1];
    }
    rows[reccnt] = NULL;
    reccnt--;
    modified = TRUE;
    flagmsg();
}

int substr(char *str1, char *str2)
{
  int i, j, len1, len2;

  len1 = (int)strlen(str1);
  len2 = (int)strlen(str2);
  for (i = 0; i < len1 - len2 + 1; i++) {
    for (j = 0; j < len2 && ((str2[j] == '?') || (str1[i+j] == str2[j])); j++);
    if (j == len2)
       return (i+1);
  }
  return (0);
}

void search(int y, int c)
{
    int i, j, k;
    char cstr[] = "?";
    char s[MAXSTRLEN+1];
    bool masked;
    struct slre_cap cap = { NULL, 0 };

    j = strlen(fstr);
    masked = (strchr(fstr, '?') != NULL);
    if (c == 0)
    {
        for (i=y+1; i<reccnt; i++)
        {
            strcpy(s, rows[i]);
            casestr(s, TRUE, TRUE);
            if (masked)
            {
                if (substr(s, fstr) > 0)
                {
                    curr = i;
                    break;
                }
            }
            else
            {
                if (strstr(s, fstr) != NULL)
                {
                    curr = i;
                    break;
                }
            }
        }
        regex = FALSE;
    }
    else if (c < 0)
    {
        if (j > 0)
        {
            if (c == -1)
            {
               j--;
               fstr[j] = '\0';
            }
            for (i=y-1; i>=0; i--)
            {
                strcpy(s, rows[i]);
                casestr(s, TRUE, TRUE);
                if (masked)
                {
                    if (substr(s, fstr) > 0)
                    {
                        curr = i;
                        break;
                    }
                }
                else
                {
                    if (strstr(s, fstr) != NULL)
                    {
                        curr = i;
                        break;
                    }
                }
                curr = (i < 0) ? 0 : curr;
            }
        }
        regex = FALSE;
    }
    else if (c == RXFORW)
    {
        for (i=y+1; i<reccnt; i++)
        {
            if ((k = slre_match(fstr, rows[i], strlen(rows[i]), &cap, 1)) > 0)
            {
                curr = i;
                break;
            }
        }
    }
    else if (c == RXBACK)
    {
        for (i=y-1; i>=0; i--)
        {
            if ((k = slre_match(fstr, rows[i], strlen(rows[i]), &cap, 1)) > 0)
            {
                curr = i;
                break;
            }
        }
    }
    else
    {
        regex = FALSE;
        cstr[0] = (char)c;
        strcat(fstr, cstr);
        casestr(fstr, TRUE, TRUE);
        j++;
        for (i=y; i<reccnt; i++)
        {
            strcpy(s, rows[i]);
            casestr(s, TRUE, TRUE);
            if (masked)
            {
                if (substr(s, fstr) > 0)
                {
                    curr = i;
                    break;
                }
            }
            else
            {
                if (strstr(s, fstr) != NULL)
                {
                    curr = i;
                    break;
                }
            }
        }
    }
    setcolor(wstat, MAINMENUCOLOR);
    mvwaddstr(wstat, 0, 20, (j!=0) ? "*" : " ");
    setcolor(wstat, FSTRCOLOR);
    mvwaddstr(wstat, 0, 21, fstr);
    setcolor(wstat, STATUSCOLOR);
    wclrtoeol(wstat);
    wrefresh(wstat);
}

void searchfield(int y, int x)
{
    int i, j, k;
    char s[MAXSTRLEN+1];
    char *p;
    struct slre_cap cap = { NULL, 0 };

    for (i=y+1; i<reccnt; i++)
    {
        strcpy(s, rows[i]);
        p = s;
        if (x == 0)
        {
            for (j=0; !(s[j]==csep || s[j]=='\0'); j++);
            s[j] = '\0';
        }
        else
        {
            for (j=0; j<x; j++)
            {
                while (!(p[0]==csep || p[0]=='\0')) p++;
                p++;
            }
            for (j=0; !(p[j]==csep || p[j]=='\0'); j++);
            p[j] = '\0';
        }
        if ((k = slre_match(fstr, p, strlen(p), &cap, 1)) > 0)
        {
            curr = i;
            break;
        }
    }
    j = strlen(fstr);
    setcolor(wstat, MAINMENUCOLOR);
    mvwaddstr(wstat, 0, 20, (j!=0) ? "*" : " ");
    setcolor(wstat, FSTRCOLOR);
    mvwaddstr(wstat, 0, 21, fstr);
    setcolor(wstat, STATUSCOLOR);
    wclrtoeol(wstat);
    wrefresh(wstat);
}

#define MAXSL 33

void getfstr(void)
{
    char *fieldname[2];
    char *fieldbuf[2];

    fieldname[0] = "Search:";
    fieldname[1] = 0;
    fieldbuf[0] = fstr;
    fieldbuf[1] = 0;

    regexp = TRUE;
    getstrings(fieldname, fieldbuf, 0, MAXSL+1);
    regexp = FALSE;
    touchwin(wbody);
    wrefresh(wbody);
    regex = TRUE;
}

void change()
{
    int i, j, k;
    static char s1[MAXSL] = "";
    static char s2[MAXSL] = "";
    char *fieldname[3];
    char *fieldbuf[3];
    char s[MAXSTRLEN+1];
    int changes=0;
    int rlen;
    char c;
    char *p;

    if (ro)
        return;

    fieldname[0] = "From:";
    fieldname[1] = "  To:";
    fieldname[2] = 0;
    if (strlen(fstr) <= MAXSL)
        strcpy(s1, fstr);
    fieldbuf[0] = s1;
    fieldbuf[1] = s2;
    fieldbuf[2] = 0;

    getstrings(fieldname, fieldbuf, 0, MAXSL+1);

    rlen = strlen(s1)-1;

    for (i=0; i<reccnt; i++)
    {
        if (s1[0] == '(' && s1[rlen] == ')')
        {
            p = slre_replace(s1, rows[i], s2);
            strcpy(rows[i], p);
            changes++;
        }
        else
        while ((k = substr(rows[i], s1)) > 0)
        {
            for (j=0; j<k; j++)
               s[j] = rows[i][j];
            s[k] = '\0';
            strcat(s, s2);
            strcat(s, rows[i]+k+strlen(s1));
            strcpy(rows[i], s);
            changes++;
        }
    }
    if (changes)
    {
        c = (changes == 1) ? ' ' : 's';
        sprintf(s, "%d occurence%c changed.", changes, c);
        msg(s);
        modified = TRUE;
        flagmsg();
        do idle(); while (!keypressed());
        touchwin(wbody);
        wrefresh(wbody);
    }
}


void copy(int y)
{
    int i, j;
    char c;

    if (ro)
        return;
    i = 0;
    j = 0;
    if (field > 0)
    {
        while ((c = rows[y][i]) != '\0')
        {
            i++;
            if (c == csep)
               j++;
            if (j == field)
               break;
        }
    }
    j = 0;
    while (1)
    {
        c = rows[y][i];
        if ((c == csep) || (c == '\0'))
            break;
        i++;
        clp[j] = c;
        j++;
    }
    clp[j] = '\0';
}

void paste(int y)
{
    int i, j, k, l;
    char c;

    if (ro)
        return;
    if (clp[0] == '\0')
        return;
    i = 0;
    j = 0;
    if (field > 0)
    {
        while ((c = rows[y][i]) != '\0')
        {
            i++;
            if (c == csep)
               j++;
            if (j == field)
               break;
        }
    }
    j = 0;
    while (1)
    {
        c = rows[y][i];
        if ((c == csep) || (c == '\0'))
        {
            l = strlen(rows[y]);
            for (k=l; k>i; k--)
                rows[y][k] = rows[y][k-1];
        }
        c = clp[j];
        if (c == '\0')
            break;
        rows[y][i] = c;
        i++;
        j++;
    }
    modified = TRUE;
    flagmsg();
}    

void reorder(int y, bool left)
{
    int i, j, k;
    char buf[MAXSTRLEN+1];
    char tmpstr[MAXCOLS+1][MAXSTRLEN+1];
    char *p;

    if (ro)
        return;

    j = left ? field-1 : field+1;

    for (i=0; i<=cols; i++)
        tmpstr[i][0] = '\0';
    i = 0;
    p = strtok(rows[y], ssep );
    while(p != NULL )
    {
        strcpy(tmpstr[i], p);
        k = strlen(tmpstr[i]);
        if (tmpstr[i][k-1] == '\n')
            tmpstr[i][k-1] = '\0';
        else if (tmpstr[i][k] == '\n')
                 tmpstr[i][k] = '\0';
        i++;
        if (i > cols)
           break;
        p = strtok(NULL, ssep );
    }
    if (p == NULL)
       strcpy(tmpstr[i], ssep);
    buf[0] = '\0';
    for (i=0; i<=cols; i++)
    {
        k = strlen(tmpstr[i])-1;
        if (tmpstr[i][k] == '\n')
           tmpstr[i][k] = '\0';
        if (i == j)
           strcat(buf, tmpstr[field]);
        else if (i == field)
           strcat(buf, tmpstr[j]);
        else
           strcat(buf, tmpstr[i]);
        strcat(buf, ssep);
        if (tmpstr[i+1][0] == '\0')
           break;
    }
    strcat(buf, "\n");
    i = strlen(buf);
    if (buf[i] == csep)
        buf[i] = '\0';
    if (rows[y] != NULL)
       free(rows[y]);
    rows[y] = (char *)malloc(i+1);
    strcpy(rows[y], buf);
    modified = TRUE;
    flagmsg();
}

void fieldcase(bool up, bool whole)
{
    char s[MAXSTRLEN+1];
    int i, j;

    if (ro)
        return;

    for (i=0, j=0; j<field; i++)
        if (rows[curr][i] == csep)
            j++;

    for (j=0; j<len[field]; j++)
        s[j] = rows[curr][i+j];
    s[j] = '\0';

    if (whole)
        casestr(s, up, FALSE);
    else
        capfstr(s, up, FALSE);

    for (j=0; j<len[field]; j++)
    {
        if (s[j] == csep)
            break;
        rows[curr][i+j] = s[j];
    }
    modified = TRUE;
    flagmsg();
}

void gorec()
{
    int i;
    char s[7] = "";
    char *fieldname[2];
    char *fieldbuf[2];

    fieldname[0] = "Goto:";
    fieldname[1] = 0;
    fieldbuf[0] = s;
    fieldbuf[1] = 0;

    itoa(curr, s, 10);
    getstrings(fieldname, fieldbuf, 0, 7);
    i = atoi(s);
    if ((i > 0) || (i <= reccnt))
        curr = i-1;
}


#ifdef NCURSES
#define ALT_INS KEY_IL
#define CTL_INS KEY_SIC
#define ALT_DEL KEY_DL
#define CTL_DEL KEY_SDC
#endif

void edit(void)
{
    int i, j;
    int r = bodylen() -1;
    int b = reccnt-bodylen();
    bool quit = FALSE;
    int ctop;
    int c;

    curr = (curr <= reccnt) ? curr : 0;
    ctop = curr;
    if (curr > 0)
    {
        for (i=0; i<curr; i++)
        {
            if ((i-ctop) >= r)
               ctop++;
        }
    }
    field = 0;
    curcol= 0;
    clsbody();
    while (!quit)
    {
        j = 0;
        for (i=ctop; i<(ctop+r); i++)
        {
            if (i < reccnt)
               displn(i, j+1);
            j++;
        }
        statusln();
        displn(curr, curr-ctop+1);
        switch (c = waitforkey())
        {
        case KEY_UP:
            if (curr > 0)
            {
               curr--;
               if (curr < ctop)
                  ctop--;
            }
            break;
        case KEY_DOWN:
            if (curr < reccnt)
            {
               curr++;
               if ((curr-ctop) >= r)
                  ctop++;
            }
            break;
        case KEY_LEFT:
            if (field > 0)
            {
               field--;
               if (curcol > 0)
                   curcol--;
            }
            else
               field = cols;
            break;
        case KEY_RIGHT:
            if (field < cols)
            {
               field++;
               i = bodywidth()-2;
               if ((beg[field] > i)
               && (beg[curcol+1] < i))
                  curcol++;
            }
            else
            {
               field = 0;
               curcol= 0;
            }
            break;
        case KEY_PPAGE:
            curr -= r;
            ctop -= r;
            if ((curr < 0) || (ctop < 0))
            {
               curr = 0;
               ctop = 0;
            }
            break;
        case KEY_NPAGE:
            curr += r;
            if (curr > reccnt)
               curr = reccnt;
            if (reccnt > r)
            {
               ctop += r;
               if (ctop > b)
                  ctop = b;
            }
            break;
        case KEY_HOME:
            curr = 0;
            ctop = 0;
            fstr[0] = '\0';
            regex = FALSE;
            memset(flags, 0, MAXROWS);
            wmove(wstat, 0, 20);
            wclrtoeol(wstat);
            wrefresh(wstat);
            break;
        case KEY_END:
            curr = reccnt;
            if (reccnt > r)
               ctop = b+1;
            else
               ctop = 0;
            break;
        case KEY_ESC:
            quit = TRUE;
            break;
/*        case KEY_ENTER:*/
        case '\n':
            if (curr < reccnt)
            {
               modify(curr);
               curs_set(1);
            }
            break;
        case CTL_INS:
            newrec(curr);
            b = reccnt-bodylen();
            break;
        case ALT_INS:
            dupl(curr);
            b = reccnt-bodylen();
            break;
        case CTL_DEL:
        case ALT_DEL:
            if (curr < reccnt)
            {
               purge(curr);
               b = reccnt-bodylen();
            }
            break;
/*        case KEY_TAB:*/
        case 9:
            if (strlen(fstr))
            {
               search(curr, regex ? RXFORW : 0);
               if ((curr-ctop) >= r)
                  ctop = curr - (r-1);
            }
            break;
        case CTL_TAB:
            if (strlen(fstr))
            {
               searchfield(curr, field);
               if ((curr-ctop) >= r)
                  ctop = curr - (r-1);
            }
            break;
/*        case KEY_BKSP:*/
        case KEY_BACKSPACE:
        case 8:
            if (strlen(fstr))
            {
               if (regex)
               {
                  i = strlen(fstr)-1;
                  if (!is_metacharacter((unsigned char *)(fstr+i)))
                      fstr[i] = '\0';
               }
               search(curr, regex ? RXBACK : -1);
               if (curr < ctop)
                  ctop = curr;
            }
            break;
        case KEY_BTAB:
            if (strlen(fstr))
            {
               search(curr, regex ? RXBACK : -2);
               if (curr < ctop)
                  ctop = curr;
            }
            break;
        case KEY_DC:
            fstr[0] = '\0';
            wmove(wstat, 0, 20);
            wclrtoeol(wstat);
            wrefresh(wstat);
            break;
#ifdef PDCURSES
        case KEY_MOUSE:
            getmouse();
                button = 0;
            request_mouse_pos();
            if (BUTTON_CHANGED(1))
                button = 1;
            else if (BUTTON_CHANGED(2))
                button = 2;
            else if (BUTTON_CHANGED(3))
                button = 3;
            if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_PRESSED)
            {
                j = MOUSE_Y_POS;
                if ((j>2) && (j<(r+3)))
                {
                    i = ctop + j -3;
                    if (i > reccnt)
                       curr = reccnt;
                    else
                       curr = i;
                    for (i=cols; i>=0; i--)
                    {
                       if (beg[i] <= MOUSE_X_POS)
                       {
                          field = i;
                          break;
                       }
                    }
                }
                else
                {
                    if (j==1)
                        quit = TRUE;
                }
            }
            if ((BUTTON_STATUS(button) &
                BUTTON_ACTION_MASK) == BUTTON_CLICKED)
            {
                if ((curr < reccnt)
                && (curr == (ctop+MOUSE_Y_POS-3)))
                {
                    modify(curr);
                    curs_set(1);
                }
            }
            break;
#endif
        case CTRL_F:
            getfstr();
            search(curr, RXFORW);
            if ((curr-ctop) >= r)
               ctop = curr - (r-1);
            break;
        case ALT_F:
            getfstr();
            searchfield(curr, field);
            if ((curr-ctop) >= r)
               ctop = curr - (r-1);
            break;
        case CTRL_C:
            copy(curr);
            break;
        case CTRL_V:
            paste(curr);
            break;
        case CTRL_X:
            if (wr == TRUE)
            {
               ro = (ro == TRUE) ? FALSE : TRUE;
               titlemsg(datfname);
            }
            break;
        case ALT_A:
            filtered = TRUE;
        case CTRL_A:
            memset(flags, 0, MAXROWS);
            if (strlen(fstr))
            {
               i = curr;
               curr = -1;
               do
               {
                  j = curr;
                  search(curr, regex ? RXFORW : 0);
                  if (curr != j)
                     flags[curr] = 1;
                  else
                     break;
               } while (j < reccnt);
               curr = i;
            }
            else
               filtered = FALSE;
            if (filtered)
            {
               fltls();
               clsbody();
               wrefresh(wbody);
            }
            break;
        case CTL_LEFT:
            if (field > 0)
               reorder(curr, TRUE);
            break;
        case CTL_RIGHT:
            if (field < cols)
               reorder(curr, FALSE);
            break;
        case ALT_LEFT:
            if (field > 0)
            {
               msg(WSTR);
               for (i=0; i<reccnt; i++)
                     reorder(i, TRUE);
               msg(NULL);
            }
            break;
        case ALT_RIGHT:
            if (field < cols)
            {
               msg(WSTR);
               for (i=0; i<reccnt; i++)
                     reorder(i, FALSE);
               msg(NULL);
            }
            break;
        case CTRL_U:
            fieldcase(TRUE, TRUE);
            break;
        case CTRL_L:
            fieldcase(FALSE, TRUE);
            break;
        case ALT_U:
            fieldcase(TRUE, FALSE);
            break;
        case ALT_L:
            fieldcase(FALSE, FALSE);
            break;
        case KEY_F(1):
            HELP;
            break;
        default:
            if ((c == 0x81) || (c == 0x1FB))
               c = 0x55; /* U */
            else if (c == 0x90)
                    c = 0x45; /* E */
            if (!iscntrl(c))
            {
               search(curr, c);
               if ((curr-ctop) >= r)
                  ctop = curr - (r-1);
            }
            break;
        }
    }
    setcolor(wbody, BODYCOLOR);
    clsbody();
    wrefresh(wbody);
    werase(wstat);
}

void redraw()
{
    register int i, j;

    clsbody();
    wrefresh(wbody);
    j = 0;
    for (i=0; i<bodylen(); i++)
    {
        if (i < reccnt)
           displn(i, j+1);
        j++;
    }
}

void selected(void)
{
    int i, j;
    FILE *fp;
    char *p;
    char tmpfname[MAXSTRLEN];
    char buf[MAXSTRLEN+1];

    if (filtered == FALSE)
    {
        msg("Not filtered!");
        waitforkey();
        msg(NULL);
        return;
    }

    strcpy(tmpfname, basename(datfname));
    p = strchr(tmpfname, '.');
    if (p != NULL)
    {
        p[0] = '\0';
    }
    strcat(tmpfname, ".$$$");
    if ((fp = fopen(tmpfname, "w")) != NULL)
    {
        msg(WSTR);
        fputs(head, fp);
        for (i=0, j=0; i<reccnt; i++)
        {
            if (flags[i])
            {
                fputs(rows[i], fp);
                j++;
            }
        }
        fclose(fp);
        msg(NULL);
        if (j == 0)
            return;
/*        execlp("tsvdb", tmpfname, 0);*/
        strcpy(buf, progname);
        strcat(buf, " ");
        strcat(buf, tmpfname);
        system(buf);
        colorbox(wtitl, TITLECOLOR, 0);
        titlemsg(datfname);
        redraw();
        flagmsg();
    }
    else
    {
        sprintf(buf, "ERROR: Can't create file '%s'", tmpfname);
        errormsg(buf);
    }
    return;
}

void dosort(void)
{
    if (ro)
        return;
    if (yesno("Sort database? (Y/N):") == 0)
        return;
    if (yesno("Reverse order? (Y/N):") == 0)
        sort(reccnt);
    else
        sort_back(reccnt);
    modified = TRUE;
    redraw();
}

int putmsg(char *s)
{
    WINDOW *wmsg;
    int i;
    
    i = strlen(s);
    wmsg = mvwinputbox(wbody, (bodylen()-5)/2, (bodywidth()-i)/2, 3, i+2);
    mvwaddstr(wmsg, 1,1, s);
    wrefresh(wmsg);
    i = toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
    return i;
}

void dosortby(void)
{
    register int i, j;
    int k, l;
    int sortidx[MAXCOLS];

    if (ro)
        return;

    k = -1;
    l = 0;
    for (i=0; i<cols; i++)
    {
        k += len[i];
        for (j=1; j<reccnt; j++)
        {
            if (rows[j][k] != csep)
                break;
        }
        if (j == reccnt)
        {
            sortidx[i] = k;
            l++;
        }
    }
    if (l > 0)
    {
        i = putmsg("Sort by field 2, 3, ... ?");
        i -= '0';
        i--;
        if ((i > 0) && (i <= l))
        {
            sortpos = sortidx[i-1];
            if (yesno("Reverse order? (Y/N):") == 0)
                sort(reccnt);
            else
                sort_back(reccnt);
            sortpos = 0;
            modified = TRUE;
            flagmsg();
            j = 0;
            for (i=0; i<bodylen(); i++)
            {
                if (i < reccnt)
                   displn(i, j+1);
                j++;
            }
            redraw();
        }
    }
}

void docrypt(void)
{
    if (locked || (ro && !crypted))
        return;
    if (yesno("Encrypt database? (Y/N):") == 0)
        return;
    crypt(reccnt);
    modified = TRUE;
    flagmsg();
}

void dosum(void)
{
    int i, j;
    double n;
    double x[MAXCOLS];
    char s[MAXCOLS][MAXSTRLEN+1];
    char buf[MAXSTRLEN+1];
    char *p=NULL;
    char *bp;
    char *fieldnam[MAXCOLS+1];
    char *fieldbuf[MAXCOLS+1];
    int flen = 40;

    for (j=0; j<MAXCOLS; j++)
        x[j] = 0.0;
    for (i=0; i<reccnt; i++)
    {
        strcpy(buf, rows[i]);
        p = strtok(buf, ssep );
        for (j=0; j<=cols; j++)
        {
            if (p != NULL)
            {
                strcpy(s[j], p);
                s[j][len[j]] = '\0';
                p = strtok( 0, ssep );
            }
            else
                strcpy(s[j], " ");
            n = strtod(s[j], &bp);
            x[j] += n;
        }
    }
    for (i = 0; i <= cols; i++)
    {
        sprintf(s[i], "%f", x[i]);
        fieldnam[i] = stru[i];
        fieldbuf[i] = s[i];
    }
    fieldnam[cols+1] = (char *)0;
    fieldbuf[cols+1] = s[cols+1];
    j = getstrings(fieldnam, fieldbuf, 0, flen);
}

void DoOpen(void)
{
    char fname[MAXSTRLEN];

    if (!ro)
    {
        if (modified == TRUE)
            if (savefile(datfname, 0) != 0)
               return;
    }
    strcpy(fname, datfname);
    if (getfname ("File to open:", fname, 50))
    {
        if (loadfile(fname) == 0)
            titlemsg(fname);
    }
}

void DoSave(void)
{
    char fname[MAXSTRLEN];

    if (ro && !cry)
        return;
    strcpy(fname, datfname);
    if (getfname ("Save to file:", fname, 50))
    {
        if (savefile(fname, 0) == 0)
            titlemsg(fname);
    }
}

void newdb(void)
{
    create(FNAME);
}

void bye(void)
{
    quit = FALSE;
    if (modified)
    {
        if (yesno("Database changed! Leave it? (Y/N):") != 0)
        {
            DoExit();
            return;
        }
        if (yesno("Save file? (Y/N):") == 0)
            return;
        if (savefile(datfname, 1) != 0)
            return;
    }
    DoExit();
}

/****DAT****/

/***************************** forward declarations ***********************/

void sub0(void), sub1(void), sub2(void);
void subfunc1(void), subfunc2(void);
void reghelp(void), edithelp(void);

/***************************** menus initialization ***********************/

menu MainMenu[] =
{
    { "File", sub0, "Open/Save/Exit" },
    { "Edit", sub1, "Edit" },
    { "Help", sub2, "Help" },
    { "", (FUNC)0, "" }   /* always add this as the last item! */
};

menu SubMenu0[] =
{
    { "Open", DoOpen, "Open data file" },
    { "New", newdb, "Create data file" },
    { "Save", DoSave, "Save data file" },
    { "Exit", bye, "Quit" },
    { "", (FUNC)0, "" }
};

menu SubMenu1[] =
{
    { "Edit", edit, "File edit" },
    { "View", brows, "Browse file" }, 
    { "Go", gorec, "Go to record" },
    { "Change", change, "Replace string" },
    { "Sort", dosort, "Sort file" },
    { "Field", dosortby, "Sort by other field" },
    { "sUm", dosum, "Aggregate" },
    { "Crypt", docrypt, "Code/decode" },
    { "eXport", selected, "Restricted set" },
    { "", (FUNC)0, "" }
};

menu SubMenu2[] =
{
    { "Keys", subfunc1, "Keys" },
    { "Input keys", edithelp, "Keys in edit mode" },
    { "Regexp", reghelp, "Regular expression help" },
    { "About", subfunc2, "Info" },
    { "", (FUNC)0, "" }
};

/***************************** main menu functions ************************/

void sub0(void)
{
    domenu(SubMenu0);
}

void sub1(void)
{
    domenu(SubMenu1);
}

void sub2(void)
{
    domenu(SubMenu2);
}

/***************************** submenu2 functions *************************/

void subfunc1(void)
{
    WINDOW *wmsg;
    char *s[] =
    {
        "  Ctrl-Ins:\tinsert line",
        "   Alt-Ins:\tduplicate",
        "  Ctrl-Del:\tdelete line",
        "     Enter:\tedit fields",
        "    Letter:\tsearch (? mask)",
        "Ctrl/Alt-F:\tregexp search",
        " Tab/C-Tab:\tfind next",
        "  Shft-Tab:\tprevious",
        "      Bksp:\tdel fstr back",
        "  Del/Home:\tclear fstr",
        "Ctrl/Alt-A:\tmark/filter all",
        "    Ctrl-C:\tcopy",
        "    Ctrl-V:\tpaste",
        "C/A-arrows:\treorder fields",
        "Ctrl/Alt-U:\tuppercase",
        "Ctrl/Alt-L:\tlowercase"
    };
    int i;
    int j=16;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/3, (bodywidth()-33)/2, j+2, 33);
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 3, s[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

void edithelp(void)
{
    WINDOW *wmsg;
    char *s[] =
    {
        "      Home:\tgo to 1'st char",
        "       End:\tgo to EOL",
        "        Up:\tprevious field",
        "      Down:\tnext field",
        "       Del:\tdelete char",
        "      Bksp:\tdelete back",
        "  Ctrl-End:\tdelete from cursor",
        "Ctl-arrows:\tskip word",
        "    Ctrl-C:\tcopy",
        "    Ctrl-V:\tpaste",
        "Ctrl/Alt-U:\tuppercase",
        "Ctrl/Alt-L:\tlowercase",
        "       Esc:\tcancel",
        "     Enter:\tmodify record"
    };
    int i;
    int j=14;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/3, (bodywidth()-36)/2, j+2, 36);
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 2, s[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

void reghelp(void)
{
    WINDOW *wmsg;
    char *s[] =
    {
        "^       Match beginning of a buffer",
        "$       Match end of a buffer",
        "()      Grouping and substring capturing",
        "\\s      Match whitespace",
        "\\S      non-whitespace",
        "\\d      decimal digit",
        "+       one or more times (greedy)",
        "+?      one or more times (non-greedy)",
        "*       zero or more times (greedy)",
        "*?      zero or more times (non-greedy)",
        "?       zero or once (non-greedy)",
        "x|y     x or y (alternation operator)",
        "\\meta   one of the meta character: ^$().[]*+?|\\",
        "\\xHH    byte with hex value 0xHH, e.g. \\x4a",
        "[...]   any character from set. Ranges like [a-z] supported",
        "[^...]  any character but ones from set"
    };
    int i;
    int j=16;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/3, (bodywidth()-62)/2, j+2, 62);
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 2, s[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

void subfunc2(void)
{
    WINDOW *wmsg;
    char *s[] =
    {
        "TSVdb v."VERSION,
        "Simple tab separated text database",
        " Based on PDCurses TUIdemo sample",
        "      (http://tsvdb.sf.net)"
    };
    
    wmsg = mvwinputbox(wbody, (bodylen()-5)/3, (bodywidth()-40)/2, 6, 40);
    mvwaddstr(wmsg, 1,13, s[0]);
    mvwaddstr(wmsg, 2, 3, s[1]);
    mvwaddstr(wmsg, 3, 3, s[2]);
    mvwaddstr(wmsg, 4, 3, s[3]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

/***************************** start main menu  ***************************/

int main(int argc, char **argv)
{
    int i, j, c;
    char s[MAXSTRLEN] = "";

    strcpy(progname, argv[0]);
    curr = 0;
    opterr = 0;
    while ((c=getopt(argc, argv, "HhRrVvXxYyTtBbN:n:D:d:S:s:?")) != -1)
    {
      switch (c)
      {
        case 'x':
        case 'X':
          wr = TRUE;
        case 'r':
        case 'R':
          ro = TRUE;
          break;
        case 'y':
        case 'Y':
          ro = TRUE;
          locked = TRUE;
          break;
        case 'v':
        case 'V':
          printf("\nTSVdb v.%s %s by begyu\n", VERSION, __DATE__);
          exit(0);
          break;
        case 't':
        case 'T':
          ontop = TRUE;
          break;
        case 'b':
        case 'B':
          bottom = TRUE;
          break;
        case 's':
        case 'S':
          strcpy(fstr, optarg);
          j = strlen(fstr);
          for (i=0; i<j; i++)
          {
              if (is_metacharacter((unsigned char *)(fstr+i)))
              {
                  regex = TRUE;
                  unkeys[3] = CTRL_F;
                  break;
              }
          }
          if (regex == FALSE)
              casestr(fstr, TRUE, TRUE);
          pfind = TRUE;
          break;
        case 'd':
        case 'D':
          if ((strcmp(optarg, COLSSEP) == 0)
          ||  (strcmp(optarg, SCLSSEP) == 0))
          {
              csep = optarg[0];
              ssep[0] = optarg[0];
          }
          break;
        case '?':
          if ((toupper(optopt) == 'S')
          || (toupper(optopt) == 'D'))
              fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
              fprintf (stderr, "Unknown option '-%c'.\n", optopt);
          else
              fprintf (stderr, "Unknown option character '%c'.\n", optopt);
/*
//        case ':':
//          if (optopt != '?')
//                printf("Missing argument '%c'.\n", optopt);
*/
        case 'n':
        case 'N':
          curr = atoi(optarg);
          if (curr > 0)
          {
              curr--;
              break;
          }
          else
          {
              fprintf (stderr, "Expected record number.\n");
              c = 'h';
          }
        case 'h':
        case 'H':
          fprintf(stderr, 
             "Usage: %s [-r] [-t|b] [-n<row>] [-s<fstr>] [-d<delim>] [datafile]\n",
             (char *)basename(progname));
          exit(c==':' ? -1 : 0);
        default:
          break;
      }
    }
    if (optind < argc)
        strcpy(s, argv[optind++]);
    else
        strcpy(s, FNAME);
    init();
    if (loadfile(s) == 0)
       strcpy(datfname, s);
    else
    {
       if (create(s) != 0)
                return -1;
    }
    setlocale(LC_ALL, "");
    signal(SIGINT, siginthandler);
    startmenu(MainMenu, datfname);
    return 0;
}
