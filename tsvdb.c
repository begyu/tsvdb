/*
 * $Id: tcsvdb.c,v 0.9.81 2016/03/24 $
 */

#define VERSION "0.9.81"
#define URL "http://tsvdb.sf.net"

#ifdef XCURSES
#include <xcurses.h>
# define FNAME  "./new.tsv"
#else
# define FNAME  ".\\new.tsv"
# include <curses.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <wchar.h>
#include <getopt.h>
#include <sys/stat.h>
/*#include <fcntl.h>*/
#include <dirent.h>

#ifndef MSDOS
  #include <libgen.h>
#endif

#ifdef __MINGW_VERSION
  #include <windows.h>
  static bool terminable = TRUE;
#endif

#ifdef __cplusplus
extern "C" {
#endif


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


int putmsg(char *, char *, char *);


/****REGEXP****/
/* http://cesanta.com */
#include "slre.c"

#define RXCINS ("(?i)")
#define RXCLEN 4

void regerr(int i, char *s)
{
  switch (i)
  {
    case SLRE_NO_MATCH:
        strcpy(s, "No match");
        break;
    case SLRE_UNEXPECTED_QUANTIFIER:
        strcpy(s, "Unexpected quantifier");
        break;
    case SLRE_UNBALANCED_BRACKETS:
        strcpy(s, "Unbalanced brackets");
        break;
    case SLRE_INTERNAL_ERROR:
        strcpy(s, "Internal error");
        break;
    case SLRE_INVALID_CHARACTER_SET:
        strcpy(s, "Invalid char set");
        break;
    case SLRE_INVALID_METACHARACTER:
        strcpy(s, "Invalid metacharacter");
        break;
    case SLRE_CAPS_ARRAY_TOO_SMALL:
        strcpy(s, "Caps array too small");
        break;
    case SLRE_TOO_MANY_BRANCHES:
        strcpy(s, "Too many branches");
        break;
    case SLRE_TOO_MANY_BRACKETS:
        strcpy(s, "Too many brackets");
        break;
    default:
        s[0] = '\0';
        break;
  }
  if (i < 0)
      putmsg("REGEXP: ", s, "!");
}

static char *slre_replace(const char *regex, const char *buf,
                          const char *sub) {
  char *s = NULL;
  int n, n1, n2, n3, s_len, len = strlen(buf);
  struct slre_cap cap = { NULL, 0 };

  do {
    s_len = s == NULL ? 0 : strlen(s);
    if ((n = slre_match(regex, buf, len, &cap, 1, 0)) > 0) {
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


#ifdef __MINGW_VERSION
static void RemoveConsoleCloseButton()
{
    HANDLE handle = GetConsoleWindow();
    HMENU hMenu = GetSystemMenu(handle, FALSE);
    EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    DrawMenuBar(handle);
}

static void EnableConsoleCloseButton()
{
    HANDLE handle = GetConsoleWindow();
    HMENU hMenu = GetSystemMenu(handle, FALSE);
    EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
    DrawMenuBar(handle);
}
#endif

//#if !defined(__unix) || defined(__DJGPP__)
void xlate(char *s, int x)
{
    char c[4][9] = {{0xA0,0x82,0xA1,0xA2,0x94,0x8B,0xA3,0x81,0xFB},
                    {0xB5,0x90,0xD6,0xE0,0x99,0x8A,0xE9,0x9A,0xEB},
                    {0xE1,0xE9,0xED,0xF3,0xF6,0xF5,0xFA,0xFC,0xFB},
                    {0xC1,0xC9,0xCD,0xD3,0xD6,0xD5,0xDA,0xDC,0xDB}};
    int i, j;
    int k = strlen(s);
   
    for (i=0; i<k; i++)
    {
        for (j=0; j<9; j++)
        {
            switch (x)
            {
            case 0:
                if (s[i] == c[0][j])
                    s[i] = c[2][j];
                else if (s[i] == c[1][j])
                    s[i] = c[3][j];
                break;
            case 1:
                if (s[i] == c[2][j])
                    s[i] = c[0][j];
                else if (s[i] == c[3][j])
                    s[i] = c[1][j];
                break;
            }         
        }         
    }
}

void cpch(char *c)
{
  int i = -1;
  char *p0;
  char *p1;
  char c0[] =
  "\xA0\x82\xA1\xA2\x94\x8B\xA3\x81\xB5\x90\xD6\xE0\x99\x8A\xE9\x9A\xEB\xDA\xCD"
  "\x27\xE1\x22\x3B\x3A\x5C\x7C\x92\x3D\x2B\x5B\x7B\x30\x29\x5D\x7D\x2D\x5F";
  char c1[] =
  "\xE1\xE9\xED\xF3\xF6\xF5\xFA\xFC\xC1\xC9\xCD\xD3\xD6\xD5\xDA\xDC\xDB\x82\x99"
  "\xA0\x27\xB5\x82\x90\xA1\x92\x7C\xA2\xE0\x8B\x8A\x94\x99\xA3\xE9\x81\x9A";

  p0 = strchr(c0, c[0]);
  if (p0 == NULL)
  {
      p1 = strchr(c1, c[0]);
      if (p1 != NULL)
      {
          i = p1 - c1;
          c[0] = c0[i];
      }
  }
  else
  {
      i = p0 - c0;
      c[0] = c1[i];
  }
}
//#endif


bool is_leap_year(int year)
{
    return (!(year % 4) && (year % 100)) || !(year % 400);
}

bool is_valid_date(const char *s)
{
    char ys[] = "1234mmdd";
    char ms[] = "56dd";
    char ds[] = "78";

    if (strlen(s) != 8)
        	return FALSE;
    strcpy(ys, s);
    ys[4] = '\0';
    strcpy(ms, s+4);
    ms[2] = '\0';
    strcpy(ds, s+6);

    int y = atoi(ys);
    int m = atoi(ms);
    int d = atoi(ds);

    if (y >= 1583 && y <= 9999 &&
        m >= 1    && m <= 12   &&
        d >= 1    && d <= 31)
    {
        if (m==2 && d>29)
            return FALSE;
        if (m==2 && d==29 && !is_leap_year(y))
            return FALSE;
        if (d==31 && (m==4 || m==6 || m==9 || m==11))
            return FALSE;
        return TRUE;
    }
    return FALSE;
}

#define MAXSNLEN 10
#define ONEDAY (60*60*24)

char *jog(char *s, int i)
{
    int slen, j;
    char fs[7];
    char ns[MAXSNLEN*2];
    bool dat = FALSE;
    struct tm tinfo;
    time_t t;

    slen = strlen(s);
    if (slen == MAXSNLEN)
    {
        for (j=0; j<MAXSNLEN; j++)
        {
            if (j==4 || j==7)
            {
                if (s[j] != '.')
                    	break;
            }
            else if (!isdigit(s[j]))
                     	break;
        }
        dat = (j == MAXSNLEN);
    }
    if (dat)
    {
        strcpy(ns, s);
        ns[4] = '\0';
        tinfo.tm_year = atoi(ns) - 1900;
        strcpy(ns, s+5);
        ns[2] = '\0';
        tinfo.tm_mon = atoi(ns) -1;
        strcpy(ns, s+8);
        tinfo.tm_mday = atoi(ns);
        tinfo.tm_hour= 0;
        tinfo.tm_min = 0;
        tinfo.tm_sec = 1;
        tinfo.tm_isdst = 0;
        t = mktime(&tinfo);
        if (t == -1)
            return s;
        t += (long) (i * ONEDAY);
        j = strftime(ns, 2*MAXSNLEN, "%Y.%m.%d", localtime(&t));
        if (j == 10);
            	strcpy(s, ns);
        return s;
    }

    if (slen == 0 || slen > MAXSNLEN)
        	return s;

    for (j=0; j<slen; j++)
    {
        if (!isspace(s[j]))
            break;
    }
    if (s[j] == '-' || s[j] == '+')
        	j++;
    for (; j<slen; j++)
    {
        if (!isdigit(s[j]))
            return s;
    }
    sprintf(fs, "\%c%3dd", '%', slen);
    sprintf(ns, fs, atoi(s)+i);
    strcpy(s, ns);
    return s;
}


// /*ED*/
// extern int ed(char *);
// 
// /*CALC*/
// extern double calcExpression(char *);
// extern bool calcerr;
#include "edcal.c"


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

#define CTRL_A 0x01
#define CTRL_B 0x02
#define CTRL_C 0x03
#define CTRL_D 0x04
#define CTRL_E 0x05
#define CTRL_F 0x06
#define CTRL_G 0x07
#define CTRL_L 0x0C
#define CTRL_O 0x0F
#define CTRL_P 0x10
#define CTRL_Q 0x11
#define CTRL_R 0x12
#define CTRL_S 0x13
#define CTRL_T 0x14
#define CTRL_U 0x15
#define CTRL_V 0x16
#define CTRL_W 0x17
#define CTRL_X 0x18
#define CTRL_Y 0x19
#define CTRL_Z 0x1A
/*
//#define ALT_C 0x1a3
//#define ALT_L 0x1aC
//#define ALT_U 0x1B5
//#define ALT_W 0x1b7
//#define ALT_X 0x1b8
//#define ALT_Y 0x1b9
*/

void subfunc1(void);
#define HELP subfunc1()
void edithelp(void);
#define EDITHLP edithelp()
void reghelp(void);
#define REGHLP reghelp()
void opthelp(void);

void changecolor(void);
void disphint(int);

/****DAT****/

#define MAXROWS  1048576
#define MAXCOLS  16
#define MAXNLEN  19
#define MAXFLEN  33

#define BUFDEF char buf[MAXSTRLEN+1]

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#define CTRL(x) ((x) & 0x1f)

typedef struct
{
    int  total;
    char *ptr[MAXROWS+1];
    char flag[MAXROWS+1];
    int idx[MAXROWS+1];
} tmprows;

static tmprows tmpdat;

static char progname[MAXSTRLEN] = "";
static char wdname[MAXSTRLEN+1] = "";
static char fnam[MAXSTRLEN] = "";
static char datfname[MAXSTRLEN] = "";
static char fstr[MAXSTRLEN] = "";
static char regstr[MAXFLEN] = "(?i)";
static char fchr = '\0';
static char head[MAXSTRLEN] = "";
static char clp[MAXSTRLEN] = "";
static char *rows[MAXROWS+1];
static char flags[MAXROWS+1];
static char stru[MAXCOLS+1][MAXSTRLEN];
static int beg[MAXCOLS+1];
static int len[MAXCOLS+1];
static int cols, reccnt, curr, field, curcol, ctop;
static bool modified=FALSE;
static bool ro = FALSE;
static bool wr = FALSE;
static bool cry = FALSE;
static bool crypted = FALSE;
static bool locked = FALSE;
static bool safe = FALSE;
static int button;
static bool ontop = FALSE;
static bool bottom = FALSE;
static bool pfind = FALSE;
static bool regex = FALSE;
static bool getregexp = FALSE;
static bool goregex = FALSE;
static int regexcase = 0;
static int unkeys[] = {KEY_RIGHT, '\n', '\n', '\t', KEY_END, KEY_UP, 0};
static int unkeypos = -1;
static int sortpos = 0;
static bool numsort = FALSE;
static bool filtered = FALSE;
static bool headspac = FALSE;
static bool hunsort = FALSE;
static bool insert = FALSE;
static bool colorset = FALSE;
static bool inside = FALSE;
static long int filesize = 0L;

#define TABCSEP '\t'
#define TABSSEP "\t"
#define COLCSEP ','
#define COLSSEP ","
#define SCLCSEP ';'
#define SCLSSEP ";"

#define RXFORW 0xFF
#define RXBACK 0xFE
#define WSTR (" Wait! ")
#define DISABLEDHOT ("aCcDFfIMNnSsT")
#define FROMSTR ("From:")
#define TOSTR   ("  To:")
#define HEADSTR ("Head:")

static char csep = TABCSEP;
static char ssep[] = TABSSEP;

void msg(char *);

int casestr(char *, bool, bool);
int capfstr(char *, bool, bool);


#define MAXUNDO 9

typedef struct
{
    char und[MAXSTRLEN+1];
    char red[MAXSTRLEN+1];
    int idx;
} undotype;

static undotype undo[MAXUNDO+1];
static int undoptr = -1;
static char undel[MAXSTRLEN+1] = "";
static int undelpos = 0;

void pushundo(int y)
{
    undoptr++;
    if (undoptr > MAXUNDO)
    {
        for (i=0; i<MAXUNDO; i++)
        {
             undo[i] = undo[i+1];
        }
        undoptr--;
    }
    strcpy(undo[undoptr].und, rows[y]);
    strcpy(undo[undoptr].red, "");
    undo[undoptr].idx = y;
}

int popundo(int y)
{
    int i = y;
    int j;

    if (undoptr >= 0)
    {
        i = undo[undoptr].idx;
        strcpy(undo[undoptr].red, rows[i]);
        j = strlen(undo[undoptr].und);
        if (rows[i] != NULL)
            free(rows[i]);
        rows[i] = (char *)malloc(j+1);
        strcpy(rows[i], undo[undoptr].und);
        undoptr--;
    }
    return i;
}

int redo(int y)
{
    int i = y;

    if (undoptr < MAXUNDO)
    {
        undoptr++;
        strcpy(undo[undoptr].und, undo[undoptr].red);
        i = popundo(y);
    }
    return i;
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
// WINDOW *bodywin(void);

void    rmerror(void);
// void    rmstatus(void);

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

int     weditstr(WINDOW *win, char *buf, int field, int lim);
WINDOW *winputbox(WINDOW *win, int nlines, int ncols);
int     getstrings(char *desc[], char *buf[], int field, int length, int lim[]);
*/

#define editstr(s,f,l)           (weditstr(stdscr,s,f,l))
#define mveditstr(y,x,s,f,l)     (move(y,x)==ERR?ERR:editstr(s,f,l))
#define mvweditstr(w,y,x,s,f,l)  (wmove(w,y,x)==ERR?ERR:weditstr(w,s,f,l))

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
# define EDITBOXTOOCOLOR  (14 | A_BOLD)
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
# define EDITBOXTOOCOLOR  (A_BOLD)
#endif


#define th 1     /* title window height */
#define mh 1     /* main menu height */
#define sh 2     /* status window height */
#define bh (LINES - th - mh - sh)   /* body window height */
#define bw COLS  /* body window width */


/******************************* STATIC ************************************/

static WINDOW *wtitl, *wmain, *wbody, *wstatus; /* title, menu, body, status win*/
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
    static BUFDEF;
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
    init_pair(EDITBOXTOOCOLOR  & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);
#endif
}

static void initcolo2(void)
{
#ifdef A_COLOR
    if (has_colors())
        start_color();
    init_pair(TITLECOLOR       & ~A_ATTR, COLOR_MAGENTA, COLOR_CYAN);      
    init_pair(MAINMENUCOLOR    & ~A_ATTR, COLOR_GREEN, COLOR_CYAN);    
    init_pair(MAINMENUREVCOLOR & ~A_ATTR, COLOR_YELLOW, COLOR_BLUE);
    init_pair(SUBMENUCOLOR     & ~A_ATTR, COLOR_BLUE, COLOR_YELLOW);    
    init_pair(SUBMENUREVCOLOR  & ~A_ATTR, COLOR_YELLOW, COLOR_BLUE);   
    init_pair(BODYCOLOR        & ~A_ATTR, COLOR_BLACK, COLOR_WHITE);      
    init_pair(STATUSCOLOR      & ~A_ATTR, COLOR_BLUE, COLOR_CYAN);   
    init_pair(INPUTBOXCOLOR    & ~A_ATTR, COLOR_BLUE, COLOR_GREEN);
    init_pair(EDITBOXCOLOR     & ~A_ATTR, COLOR_RED, COLOR_WHITE);
    init_pair(CURRCOLOR        & ~A_ATTR, COLOR_CYAN, COLOR_BLUE);
    init_pair(CURRREVCOLOR     & ~A_ATTR, COLOR_YELLOW, COLOR_MAGENTA);
    init_pair(MARKCOLOR        & ~A_ATTR, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(FSTRCOLOR        & ~A_ATTR, COLOR_YELLOW, COLOR_CYAN);
    init_pair(EDITBOXTOOCOLOR  & ~A_ATTR, COLOR_YELLOW, COLOR_CYAN);
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
    BUFDEF;
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
            if (((c = strchr(DISABLEDHOT, (char)(p->name[0]))) != NULL)
               && (!strstr(p->name, "Col")) && (!strstr(p->name, "seL")))
                setcolor(wmenu, INPUTBOXCOLOR);
            else
                setcolor(wmenu, SUBMENUCOLOR);
            if (locked)
            {
                if (strstr(p->name, "seL")
                ||  strstr(p->name, "Sor")
                ||  strstr(p->name, "Fie")
                ||  strstr(p->name, "nUm"))
                    	setcolor(wmenu, SUBMENUCOLOR);
            }
        }
        else
        {
            if ((p->name[1] == 'X') && (datfname[strlen(datfname)-3] == '$'))
                setcolor(wmenu, INPUTBOXCOLOR);
            else
                setcolor(wmenu, SUBMENUCOLOR);
            if (safe && ((p->name[0] == 'M') || strstr(p->name, "Tex")))
                setcolor(wmenu, INPUTBOXCOLOR);
        }
        if (!has_colors())
        {
            if (strstr(p->name, "Col"))
                	setcolor(wmenu, INPUTBOXCOLOR);
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
            if (MOUSE_WHEEL_UP)
            {
                cur = (cur + nitems - 1) % nitems;
                key = ERR;
            }
            else if (MOUSE_WHEEL_DOWN)
            {
                cur = (cur + 1) % nitems;
                key = ERR;
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
        if (strstr(datfname, "$$$") != NULL)
        {
           wclear(wbody);
           touchwin(wbody);
           wrefresh(wbody);
        }
        delwin(wtitl);
        delwin(wmain);
        delwin(wbody);
        delwin(wstatus);
        curs_set(1);
        endwin();
        incurses = FALSE;
#ifdef PDCURSES
        mouse_set(0L);
        PDC_save_key_modifiers(FALSE);
        PDC_return_key_modifiers(FALSE);
#endif
    }
    chdir(wdname);
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

//WINDOW *bodywin(void)
//{
//    return wbody;
//}

void rmerror(void)
{
    rmline(wstatus, 0);
}

//void rmstatus(void)
//{
//    rmline(wstatus, 1);
//}

void titlemsg(char *msg)
{
    if (cry)
       mvwaddstr(wtitl, 0, 1, "C+");
    else if (ro)
       mvwaddstr(wtitl, 0, 1, "R+");
    else if (wr)
       mvwaddstr(wtitl, 0, 1, "W+");
    else if (safe)
       mvwaddstr(wtitl, 0, 1, "S+");
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
    mvwaddstr(wstatus, 0, 2, padstr(msg, bw - 3));
    wrefresh(wstatus);
}

void statusmsg(char *msg)
{
    mvwaddstr(wstatus, 1, 2, padstr(msg, bw - 3));
    wrefresh(wstatus);
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

#ifdef __MINGW_VERSION
void pause(unsigned int secs)
{
    time_t rettime = time(0) + secs;
    while (time(0) < rettime);
}
#endif

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
                    if (((c = strchr(DISABLEDHOT, (char)(mp[old].name[0]))) != NULL)
                       && (!strstr(mp[old].name, "Col"))
                       && (!strstr(mp[old].name, "seL")))
                        setcolor(wmenu, INPUTBOXCOLOR);
                    else
                        setcolor(wmenu, SUBMENUCOLOR);
                    if (locked)
                    {
                        if (strstr(mp[old].name, "seL")
                        ||  strstr(mp[old].name, "Sor")
                        ||  strstr(mp[old].name, "Fie")
                        ||  strstr(mp[old].name, "nUm"))
                            	setcolor(wmenu, SUBMENUCOLOR);
                    }
                }
                else
                {
                    if ((mp[old].name[1] == 'X') 
                    && (datfname[strlen(datfname)-3] == '$'))
                        setcolor(wmenu, INPUTBOXCOLOR);
                    else
                        setcolor(wmenu, SUBMENUCOLOR);
                    if (safe && ((mp[old].name[0] == 'M')
                                 || strstr(mp[old].name, "Tex")))
                        setcolor(wmenu, INPUTBOXCOLOR);
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
            if (MOUSE_WHEEL_UP)
            {
                key = KEY_UP;
                continue;
            }
            else if (MOUSE_WHEEL_DOWN)
            {
                key = KEY_DOWN;
                continue;
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
    wstatus = subwin(stdscr, sh, bw, th + mh + bh, 0);

    colorbox(wtitl, TITLECOLOR, 0);
    colorbox(wmain, MAINMENUCOLOR, 0);
    colorbox(wbody, BODYCOLOR, 0);
    colorbox(wstatus, STATUSCOLOR, 0);

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
    leaveok(wstatus, TRUE);
#ifdef PDCURSES
    mouse_set(ALL_MOUSE_EVENTS);
    PDC_save_key_modifiers(TRUE);
    PDC_return_key_modifiers(TRUE);
#endif
    newterm(getenv("TERM"), stderr, stdin);
    keypad(stdscr,  TRUE);
    refresh();
    if (getcwd(wdname, MAXSTRLEN) == NULL)
        strcpy(wdname, ".");
}

void startmenu(menu *mp, char *mtitle)
{
    if (mtitle)
        titlemsg(mtitle);

    mainmenu(mp);

    cleanup();
}

static void repainteditbox(WINDOW *win, int x, char *buf, int lim)
{
#ifndef PDCURSES
    int maxy;
#endif
    int maxx;
#ifdef XCURSES
    int i;
#endif

#ifdef PDCURSES
    maxx = getmaxx(win);
#else
    getmaxyx(win, maxy, maxx);
#endif
    werase(win);
#ifndef XCURSES
    mvwprintw(win, 0, 0, "%s", padstr(buf, maxx));
#else
    wmove(win, 0, 0);
    padstr(buf, maxx);
    for (i=0; i<maxx; i++)
    {
        if (buf[i] == 0)
            break;
        waddch(win, (unsigned char)(buf[i]));
    }
    for (; i<maxx; i++)
        waddch(win, ' ');
#endif
    if (lim)
    {
        lim--;
        setcolor(win, EDITBOXTOOCOLOR);
#ifndef XCURSES
        mvwprintw(win, 0, lim, "%s", padstr(buf+lim, maxx));
#else
        wmove(win, 0, lim);
        padstr(buf+lim, maxx);
        for (i=0; i<maxx; i++)
        {
            if (buf[i+lim] == 0)
                break;
            waddch(win, (unsigned char)(buf[i+lim]));
        }
        for (; i<maxx; i++)
            waddch(win, ' ');
#endif
        setcolor(win, EDITBOXCOLOR);
    }
    wmove(win, 0, x);
    wrefresh(win); 
}


int strtrimcmp(const char *s1, const char *s2)
{
    while (*s1++ == *s2++)
    {
        if ((*s1 == '\0') || (*s1 == ' ' && s1[1] < '!'))
             	return 0;
    }
    return -1;
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

int weditstr(WINDOW *win, char *buf, int field, int lim)
{
    char org[MAXSTRLEN], *tp, *bp = buf;
    bool defdisp = TRUE, stop = FALSE;
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
    curs_set(insert ? 2 : 1);

    while (!stop)
    {
        idle();
        repainteditbox(wedit, bp - buf, buf, lim);

        switch (c = wgetch(wedit))
        {
        case ERR:
            break;

        case 129: //0x81=''
        case 144: //0x90=''
        case 395: //0x8B='‹'
        case 394: //0x8A='Š'
        case 507: //0xFB='ű'
        case 491: //0xEB='ë'
            goto ins_char;
            break;

        case KEY_ESC:
            i = strtrimcmp(org, buf);
            if (i == -1)
            {
                strcpy(buf, org);   /* restore original */
                i = strlen(buf)-1;
                while ((i>0) && (buf[i]==' '))
                {
                    buf[i] = '\0';
                    i--;
                }
                if (i != -1)
                    	break;
            }
            stop = TRUE;
            break;

        case '\n':
        case KEY_UP:
        case KEY_DOWN:
        case '\t':
            stop = TRUE;
            break;

        case CTL_PGUP:
        case SHF_PADPLUS:
            jog(buf, 1);
            break;
        case CTL_PGDN:
        case SHF_PADMINUS:
            jog(buf, -1);
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
        case CTL_PADPLUS:
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
            defdisp = FALSE;
            while ((bp - buf < (int)strlen(buf)) && (*(bp) != ' ')) 
                bp++;
            while ((bp - buf < (int)strlen(buf)) && (*(bp) == ' ')) 
                bp++;
            break;
        case CTL_END:
            *bp = '\0';
            break;
        case KEY_PPAGE:
        case KEY_NPAGE:
            stop = TRUE;
            break;
        case CTL_UP:
            if (inside && (ctop > 0))
            {
                ctop--;
                disphint(curr);
                touchwin(win);
                wrefresh(win);
            }
            break;
        case CTL_DOWN:
            if (inside && (ctop < (reccnt-(bodylen()-1))))
            {
                ctop++;
                disphint(curr);
                touchwin(win);
                wrefresh(win);
            }
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
            if (MOUSE_WHEEL_UP)
            {
                c = KEY_UP;
                stop = TRUE;
            }
            else if (MOUSE_WHEEL_DOWN)
            {
                c = KEY_DOWN;
                stop = TRUE;
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
            if (i > 0)
                clp[i] = '\0';
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
                for (i=0; i<=j; i++)
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
                if (is_valid_date(buf))
                {
                    buf[10] = '\0';
                    buf[9] = (char)buf[7];
                    buf[8] = (char)buf[6];
                    buf[7] = (char)'.';
                    buf[6] = (char)buf[5];
                    buf[5] = (char)buf[4];
                    buf[4] = (char)'.';
                }
            }
            defdisp = FALSE;
            break;
        case ALT_D:
            j = strlen(buf);
            for (i=0; i<j; i++)
            {
                if (buf[i] == '.')
                    buf[i] = ',';
                else if (buf[i] == ',')
                    buf[i] = '.';
            }
            defdisp = FALSE;
            break;
        case ALT_C:
            calcexp(buf);
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
            if (getregexp == TRUE)
               REGHLP;
            else
               EDITHLP;
            touchwin(win);
            wrefresh(win);
            wrefresh(wedit);
            break;
//#if !defined(__unix) || defined(__DJGPP__)
        case KEY_SUP:
            xlate(buf, 0);
            break;
        case KEY_SDOWN:
            xlate(buf, 1);
            break;
        case CTRL_X:
            cpch(bp);
            break;
//#endif
/*#ifdef DJGPP*/
        case CTRL_Y:
            i = 0;
            tp = buf;
            while (tp[i] != '\0')
            {
                switch (tp[i])
                {
                case ',':
                    tp[i] = '.';
                    break;
                case '.':
                    tp[i] = ';';
                    break;
                case ';':
                    tp[i] = ':';
                    break;
                case ':':
                    tp[i] = ',';
                    break;
                case '"':
                    tp[i] = '\'';
                    break;
                case '\'':
                    tp[i] = '"';
                    break;
                case '(':
                    tp[i] = '[';
                    break;
                case ')':
                    tp[i] = ']';
                    break;
                case '[':
                    tp[i] = '{';
                    break;
                case ']':
                    tp[i] = '}';
                    break;
                case '{':
                    tp[i] = '<';
                    break;
                case '}':
                    tp[i] = '>';
                    break;
                case '<':
                    tp[i] = '(';
                    break;
                case '>':
                    tp[i] = ')';
                    break;
                }
                i++;
            }
            break;
/*#endif //DJGPP*/
        case ALT_P:
            stop = TRUE;
            break;
        case CTRL_O:
            c = 245;
            goto ins_char;
        case CTRL_P:
            c = 213;
            goto ins_char;
        case CTRL_R:
            c = 251;
            goto ins_char;
        case CTRL_T:
            c = 219;
            goto ins_char;
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
                     || (c==0x81 || c==0x90 || c==0xEB))
            {
ins_char:
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
    repainteditbox(wedit, bp - buf, buf, 0);
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

int getstrings(char *desc[], char *buf[], int field, int length, int lim[])
{
    WINDOW *winput;
    int oldy, oldx, maxy, maxx, nlines, ncols, i, n, l, mmax = 0;
    int c = 0;
    bool stop = FALSE;
    bool fsel = FALSE;

    if (field == -1)
    {
        field = 0;
        fsel = TRUE;
    }

    getyx(wbody, oldy, oldx);
    getmaxyx(wbody, maxy, maxx);

    i = (maxx/2)-2;
    for (n = 0; desc[n]; n++)
    {
        if (field == 0)
        {
            if ((l = strlen(desc[n])) > mmax)
                mmax = l;
        }
        else
        {
            if ((l = strlen(desc[n])) > i)
            {
                l = i-2;
                desc[n][l] = (char)0;
                length = l;
            }
            if ((l = strlen(buf[n])) > mmax)
                mmax = l;
            if (l > i)
            {
                l = i-2;
                mmax = l;
                buf[n][l-1] = (char)0;
                length = l;
            }
        }
    }

    nlines = n + 2; ncols = mmax + length + 4;
    winput = mvwinputbox(wbody, (maxy - nlines) / 2, (maxx - ncols) / 2, 
        nlines, ncols);

restart:
    for (i = 0; i < n; i++)
    {
        setcolor(winput, INPUTBOXCOLOR);
        mvwprintw(winput, i + 1, 2, "%s", desc[i]);
        setcolor(winput, SUBMENUCOLOR);
        mvwprintw(winput, i + 1, mmax + 3, "%s", padstr(buf[i], length));
    }
    setcolor(winput, INPUTBOXCOLOR);

    i = field;

    while (!stop)
    {
        l = (lim == NULL) ? 0 : lim[i];
        if (strcmp(desc[i], FROMSTR) == 0)
            getregexp = TRUE;
        else if (strcmp(desc[i], TOSTR) == 0)
                 getregexp = FALSE;
        switch (c = mvweditstr(winput, i+1, mmax+3, buf[i], length, l))
        {
        case KEY_ESC:
            stop = TRUE;
            break;

        case KEY_UP:
            i = (i + n - 1) % n;
            if (fsel)
                stop = TRUE;
            break;

        case '\n':
            if (++i == n)
                stop = TRUE;    /* all passed? */
            break;
        case '\t':
        case KEY_DOWN:
            if (++i == n)
                i = 0;
            if (fsel)
                stop = TRUE;
            break;
        case KEY_PPAGE:
        case KEY_NPAGE:
            if (fsel == FALSE)
                stop = TRUE;
            break;
        case ALT_P:
            changecolor();
            colorbox(winput, INPUTBOXCOLOR, 1);
            goto restart;
        }
    }

    delwin(winput);
    touchwin(wbody);
    wmove(wbody, oldy, oldx);
    wrefresh(wbody);

    return c;
}


int getdir(char *fname, int length)
{
    int i, ret;
    int fcnt = 0;
    char *fieldname[2];
    char *fieldbuf[2];
    fieldname[0] = "Open directory:";
    fieldname[1] = 0;
    fieldbuf[0] = fname;
    fieldbuf[1] = 0;
    struct dirent *de = NULL;
    DIR *d = opendir(".");

    if (((de = readdir(d)) != NULL)
       && (de->d_type == DT_DIR)
       && (strcmp(de->d_name, ".") == 0))
    {
       if (((de = readdir(d)) != NULL)
          && (de->d_type == DT_DIR))
       {
           fcnt++;
           strcpy(fname, de->d_name);
       }
    }
    while (1)
    {
        statusmsg("Use down/up keys to select next/previous directory");
        ret = getstrings(fieldname, fieldbuf, -1, length, NULL);
        if ((ret == KEY_UP) || (ret == KEY_DOWN))
        {
            if (ret == KEY_UP)
            {
                rewinddir(d);
                fcnt = fcnt > 0 ? fcnt-2 : 0;
                if (fcnt > 0)
                {
                    i = fcnt;
                    while ((de = readdir(d)) != NULL)
                    {
                        if (de->d_type == DT_DIR)
                        {
                           i--;
                           if (i == 0)
                              	break;
                        }
                    }
                }
            }
            while ((de = readdir(d)) != NULL)
            {
                if (de->d_type == DT_DIR)
                {
                    fcnt++;
                    if(strlen(de->d_name) < length)
                    {
                        strcpy(fname, de->d_name);
                        break;
                    }
                }
            }
        }
        else
            	break;
    }
    return (ret == KEY_ESC) ? -1 : 0;
}


void putfselhlp(int i)
{
    BUFDEF;

    strcpy(buf, "Use ");
    if (i != 2)
        	strcat(buf, "down");
    if (i == 1)
        	strcat(buf, "/");
    if (i != 0)
        	strcat(buf, "up");
    strcat(buf, " key");
    if (i == 1)
        	strcat(buf, "s");
    strcat(buf, " to select ");
    if (i != 2)
        	strcat(buf, "next");
    if (i == 1)
        	strcat(buf, "/");
    if (i != 0)
        	strcat(buf, "previous");
    strcat(buf, " file or dot ('.') to directory");
    statusmsg(buf);
}

char *getfname(bool fn, char *desc, char *fname, int length)
{
    int i, ret;
    int fcnt = 0;
    int fpos = 0;
    char *fieldname[2];
    char *fieldbuf[2];
    fieldname[0] = desc;
    fieldname[1] = 0;
    fieldbuf[0] = fname;
    fieldbuf[1] = 0;
    struct dirent *de = NULL;
    DIR *d = opendir(".");

    if (!fn)
        return (getstrings(fieldname, fieldbuf, 0, length, NULL)
                == KEY_ESC) ? NULL : fname;

    chdir(wdname);
    while (1)
    {
        putfselhlp(fpos);
        ret = getstrings(fieldname, fieldbuf, -1, length, NULL);
        if (strcmp(fname, ".") == 0)
        {
            ret = getdir(fname, length-1);
            if (ret == 0)
            {
                chdir(fname);
                d = opendir(".");
                fcnt = fpos = 0;
                ret = KEY_DOWN;
            }
        }
        else if ((strcmp(fname, "..") == 0)
             || (strcmp(fname, "/") == 0))
             {
                 chdir(fname);
                 d = opendir(".");
                 fcnt = fpos = 0;
                 ret = KEY_DOWN;
             }
        if ((ret == KEY_UP) || (ret == KEY_DOWN))
        {
            fpos = 1;
            if (ret == KEY_UP)
            {
                rewinddir(d);
                fcnt = fcnt > 0 ? fcnt-2 : 0;
                if (fcnt > 0)
                {
                    i = fcnt;
                    while ((de = readdir(d)) != NULL)
                    {
                        if (de->d_type == DT_REG)
                        {
                            i--;
                            if (i == 0)
                               	break;
                        }
                    }
                }
                fpos = (fcnt > 0) ? 1 : 0;
            }
            while ((de = readdir(d)) != NULL)
            {
                if (de->d_type == DT_REG)
                {
                    fcnt++;
                    if(strlen(de->d_name) < length)
                    {
                        strcpy(fname, de->d_name);
                        break;
                    }
                }
            }
            if (de == NULL)
                	fpos = 2;
        }
        else
            	break;
    }
    return (ret == KEY_ESC) ? NULL : fname;
}


/****DAT****/

int casestr(char *str, bool upper, bool ascii)
{
#define MAXCHS 18
  char chr_lo[] = {0xA0,0x82,0xA1,0xA2,0x94,0x8B,0xA3,0x81,0xFB,
                   0xE1,0xE9,0xED,0xF3,0xF6,0xF5,0xFA,0xFC,0xFB};
  char chr_hi[] = {0xB5,0x90,0xD6,0xE0,0x99,0x8A,0xE9,0x9A,0xEB,
                   0xC1,0xC9,0xCD,0xD3,0xD6,0xD5,0xDA,0xDC,0xDB};
  char asc_lo[] = "aeiooouuuaeiooouuu";
  char asc_hi[] = "AEIOOOUUUAEIOOOUUU";
/*  char chr_utflo[] = "ĂˇĂ©Ă­ĂłĂ¶Ĺ‘ĂşĂĽĹ±";*/
/*  char chr_utfhi[] = "ĂĂ‰ĂŤĂ“Ă–ĹĂšĂśĹ°";*/
  register int i, j;
  int w=strlen(str);
  unsigned char c;

  for (i=0; i<w; i++)
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
  register int i, j;
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
    register int i, j, k;
    char c, c1, c2, c3;
    char e1[MAXSTRLEN];
    char e2[MAXSTRLEN];
#define MAXCH 60
    char abc[]="Aµ BCCDDE‚FGGHIÖˇJKLLMNNOŕ˘™”Š‹PQRSSTTUéŁšëűVWXYZZ[\\]^_`„";
    char abd[]="AAABCDEFGGGHIJKLLLMNOPQRSTTTUUUUVWXYZabcccddddefghijklmnopA";

    if (hunsort == FALSE)
    {
        strcpy(e1, s1);
        strcpy(e2, s2);
        casestr(e1, TRUE, TRUE);
        casestr(e2, TRUE, TRUE);
        return strcmp(e1, e2);
    }

    i = j = 0;
    do
    {
        c1 = s1[i];
        if (c1>='a' && c1<='z')
            	c1 -= ('a'-'A');
        c = c1;
        for (k=0; k<MAXCH; k++)
        {
            if (c1 == abc[k])
            {
                c = abd[k];
                break;
            }
        }
        c2 = toupper(s1[i+1]);
        c3 = toupper(s1[i+2]);
        switch (c1)
        {
        case 'C':
           if (c2 == 'S')
           {
               i++;
               c++;
           }
           else if ((c2 == 'C') && (c3 == 'S'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'D':
           if (c2 == 'Z')
           {
               i++;
               c++;
           }
           else if ((c2 == 'D') && (c3 == 'Z'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'G':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'G') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'L':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'L') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'N':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'N') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'T':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'T') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'S':
           if (c2 == 'Z')
           {
               i++;
               c++;
           }
           else if ((c2 == 'S') && (c3 == 'Z'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'Z':
           if (c2 == 'S')
           {
               i++;
               c++;
           }
           else if ((c2 == 'Z') && (c3 == 'S'))
                {
                    i++; i++;
                    c++;
                }
           break;
        default:
           break;
        }
        e1[j] = c;
        i++;
        j++;
    } while (c1 != 0);

    i = j = 0;
    do
    {
        c1 = s2[i];
        if (c1>='a' && c1<='z') 
            	c1 -= ('a'-'A');
        c = c1;
        for (k=0; k<MAXCH; k++)
        {
            if (c1 == abc[k])
            {
                c = abd[k];
                break;
            }
        }
        c2 = toupper(s2[i+1]);
        c3 = toupper(s2[i+2]);
        switch (c1)
        {
        case 'C':
           if (c2 == 'S')
           {
               i++;
               c++;
           }
           else if ((c2 == 'C') && (c3 == 'S'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'D':
           if (c2 == 'Z')
           {
               i++;
               c++;
           }
           else if ((c2 == 'D') && (c3 == 'Z'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'G':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'G') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;

        case 'L':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'L') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'N':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'N') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'T':
           if (c2 == 'Y')
           {
               i++;
               c++;
           }
           else if ((c2 == 'T') && (c3 == 'Y'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'S':
           if (c2 == 'Z')
           {
               i++;
               c++;
           }
           else if ((c2 == 'S') && (c3 == 'Z'))
                {
                    i++; i++;
                    c++;
                }
           break;
        case 'Z':
           if (c2 == 'S')
           {
               i++;
               c++;
           }
           else if ((c2 == 'Z') && (c3 == 'S'))
                {
                    i++; i++;
                    c++;
                }
           break;
        default:
           break;
        }
        e2[j] = c;
        i++;
        j++;
    } while (c1 != 0);

    return strcmp(e1, e2);
}

int qsort_stringlist(const void *e1, const void *e2)
{
    int i = 0;
    int j = 0;
    register int k;
    char *p1 = *(char **)(e1);
    char *p2 = *(char **)(e2);
    char s1[MAXNLEN+1] = "";
    char s2[MAXNLEN+1] = "";
    double n1, n2;


    k =0;
    for (i=0; k<sortpos; i++)
    {
        if (p1[i] == csep)
            k++;
        else if (p1[i] == '\0')
                 return (-1);
    }
    k =0;
    for (j=0; k<sortpos; j++)
    {
        if (p2[j] == csep)
            k++;
        else if (p2[j] == '\0')
                 return (+1);
    }
    if (numsort)
    {
        while (p1[i] == ' ') { i++; }
        while (p2[j] == ' ') { j++; }
        for (k=0; k<MAXNLEN; k++)
        {
            s1[k] = p1[i+k];
            if ((s1[k] == csep) || isspace(s1[k]) || (s1[k] == '\0'))
            {
                s1[k] = '\0';
                break;
            }
            if (s1[k] == ',')
                s1[k] = '.';
        }
        s1[MAXNLEN] = '\0';
        for (k=0; k<MAXNLEN; k++)
        {
            s2[k] = p2[j+k];
            if ((s2[k] == csep) || isspace(s2[k]) || (s2[k] == '\0'))
            {
                s2[k] = '\0';
                break;
            }
            if (s2[k] == ',')
                s2[k] = '.';
        }
        s2[MAXNLEN] = '\0';
        n1 = atof(s1);
        n2 = atof(s2);
        if ((n1 != 0.0) && (n2 != 0.0))
            	return (n1 < n2) ? -1 : 1;
    }
    return hstrcmp(*(char **)(e1)+i, *(char **)(e2)+j);
}

void sort(int n)
{
    msg(WSTR);
    qsort(rows, n, sizeof(char *), qsort_stringlist);
    msg(NULL);
}

int qs_stringlist_rev(const void *e1, const void *e2)
{
    int i = 0;
    int j = 0;
    register int k;
    char *p1 = *(char **)(e1);
    char *p2 = *(char **)(e2);
    char s1[MAXNLEN+1] = "";
    char s2[MAXNLEN+1] = "";
    double n1, n2;

    k =0;
    for (i=0; k<sortpos; i++)
    {
        if (p2[i] == csep)
            k++;
        else if (p2[i] == '\0')
                 return (-1);
    }
    k =0;
    for (j=0; k<sortpos; j++)
    {
        if (p1[j] == csep)
            k++;
        else if (p1[j] == '\0')
                 return (+1);
    }
    if (numsort)
    {
        while (p2[i] == ' ') { i++; }
        while (p1[j] == ' ') { j++; }
        for (k=0; k<MAXNLEN; k++)
        {
            s1[k] = p2[i+k];
            if ((s1[k] == csep) || isspace(s1[k]) || (s1[k] == '\0'))
            {
                s1[k] = '\0';
                break;
            }
            if (s1[k] == ',')
                s1[k] = '.';
        }
        s1[MAXNLEN] = '\0';
        for (k=0; k<MAXNLEN; k++)
        {
            s2[k] = p1[j+k];
            if ((s2[k] == csep) || isspace(s2[k]) || (s2[k] == '\0'))
            {
                s2[k] = '\0';
                break;
            }
            if (s2[k] == ',')
                s2[k] = '.';
        }
        s2[MAXNLEN] = '\0';
        n1 = atof(s1);
        n2 = atof(s2);
        if ((n1 != 0.0) && (n2 != 0.0))
            	return (n1 < n2) ? -1 : 1;
    }
    return hstrcmp(*(char **)(e2)+i, *(char **)(e1)+j);
}

void sort_back(int n)
{
    msg(WSTR);
    qsort(rows, n, sizeof(char *), qs_stringlist_rev);
    msg(NULL);
}

void flagmsg(void)
{
    if (modified)
    {
        setcolor(wtitl, FSTRCOLOR);
        mvwaddstr(wtitl, 0, 0, "*");
#ifdef __MINGW_VERSION
        if (terminable)
        {
            RemoveConsoleCloseButton();
            terminable = FALSE;
        }
#endif
    }
    else
    {
        setcolor(wtitl, TITLECOLOR);
        mvwaddstr(wtitl, 0, 0, "_");
    }
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
    register int i, j, k;
    int maxlen;
    char s[MAXCOLS][MAXSTRLEN+1];
    BUFDEF;
    char delim[] = "?\n";
    char *p=NULL;

    maxlen = bodywidth()-2;
    j = strlen(rows[y]);
    if (beg[curcol] < j)
    {
        i = 0; 
        j = 0; 
        while (j < curcol)
        {
            if (rows[y][i] == csep)
                j++;
            i++;
        }
        strcpy(buf, rows[y]+i);
        k = strlen(buf);
        for (i=0; i<k; i++)
        {
            if ((buf[i] == csep) && (buf[i+1] == csep))
            {
                k++;
                for (j=k; j>i; j--)
                    buf[j] = buf[j-1];
                i++;
                buf[i] = ' ';
            }
        }
    }
    else
        buf[0] = '\0';
    delim[0] = csep;
    p = strtok(buf, delim);
    for (i=0; i<=cols; i++)
    {
        if (p != NULL)
        {
            strcpy(s[i], p);
            k = strlen(s[i]);
            if (k >= len[i])
                s[i][len[i]] = '\0';
            for (j=len[i]; j>k; j--)
            {
                  strcat(s[i], " ");
            }
            p = strtok( 0, delim);
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
        if ((beg[i]+len[i]) <= maxlen)
        {
#ifndef XCURSES
           mvwaddstr(wbody, r, beg[i], s[i]);
#else
           wmove(wbody, r, beg[i]);
           k = strlen(s[i]);
           for (j=0; j<MIN(k,len[i]); j++)
               waddch(wbody, (unsigned char)(s[i][j]));
#endif
           mvwaddstr(wbody, 0, beg[i], stru[i+curcol]);
        }
        else
        {
           j = maxlen - (beg[i]+len[i]);
           if (j > 0)
           {
               s[i][j] = '\0';
               mvwaddstr(wbody, r, beg[i], s[i]);
               strcpy(buf, stru[i+curcol]);
               buf[j] = '\0';
               mvwaddstr(wbody, 0, beg[i], stru[i+curcol]);
           }
        }
    }
    j = beg[field+1];
    if (j > maxlen || (j==0 && field<cols))
    {
        setcolor(wbody, MARKCOLOR);
        mvwaddstr(wbody, r, maxlen, ">");
    }
    else
    {
        setcolor(wbody, BODYCOLOR);
        mvwaddstr(wbody, r, maxlen, " ");
    }
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
    wmove(wbody, r, beg[field]);
    wrefresh(wbody);
}

void statusln(void)
{
    BUFDEF;

    sprintf(buf, "%5u/%u (%u/%u) ", curr+1, reccnt, field+1, cols+1);
    setcolor(wbody, STATUSCOLOR);
    mvwaddstr(wstatus, 0, 0, buf);
    mvwaddstr(wstatus, 1, 0, ">");
    statusmsg(rows[curr]);
}


int RLEC(const char *in, char *out)
{
    unsigned char cur = 0;
    unsigned char count = 0;
    int inpos = 0;
    int outpos = 0;
    int i = 0;
    int inlen = strlen(in);

    while(inpos < inlen)
    {
        cur = in[inpos];
        count = 1;
        for(i = inpos + 1; i < inlen; i++)
        {
            if(in[i] == cur)
            {
                count++;
                if(count > 80)
                    break;
            }
            else
                break;
        }
        inpos += count;
        if (count > 3)
        {
            out[outpos] = 6;
            out[outpos + 1] = count + '0';
            out[outpos + 2] = cur;
            outpos += 3;
        }
        else
        {
            for (i=0; i<count; i++)
            {
                out[outpos] = cur;
                outpos++;
            }
        }
    }
    out[outpos] = 0;
    return outpos;
}

int RLED(const char *in, char *out, int outlen)
{
    unsigned char cur = 0;
    unsigned char count = 0;
    int inpos = 0;
    int outpos = 0;
    int i = 0;
    int inlen = strlen(in);

    while(inpos < inlen)
    {
        cur = in[inpos];
        if (cur == 6)
        {
            count = in[inpos + 1] - '0';
            cur = in[inpos + 2];
            for(i = 1; i < count; i++)
            {
                out[outpos] = cur;
                outpos++;
                if((outpos + i) >= outlen)
                    break;
            }
            inpos += 2;
        }
        out[outpos] = cur;
        inpos++;
        outpos++;
    }
    out[outpos] = 0;
    return outpos;
}


int numcompr(const char *s3, char *s2)
{
    unsigned int i;

    if (!(isdigit(s3[0]) && isdigit(s3[1]) && isdigit(s3[2])))
        	return -1;
#ifdef DJGPP
    if ((s3[0] == 'ˇ') || (s3[1] == 'ˇ') || (s3[2] == 'ˇ'))
        	return -1;
#endif

    i = 100 * (s3[0] - '0');
    i += 10 * (s3[1] - '0');
    i += (s3[2] - '0');
    if (i < 224)
    {
        s2[0] = 1;
        s2[1] = i+32;
    }
    else if (i < 448)
    {
        s2[0] = 2;
        s2[1] = i-192;
    }
    else if (i < 672)
    {
        s2[0] = 3;
        s2[1] = i-416;
    }
    else if (i < 896)
    {
        s2[0] = 4;
        s2[1] = i-640;
    }
    else
    {
        s2[0] = 5;
        s2[1] = i-864;
    }
    return 0;
}

void numextr(const char *s, char *dst)
{
    int i,j;
    unsigned int d,t,z;
    unsigned char c;

    i = j = 0;
    while (i < strlen(s))
    {
        c = s[i];
        d = (unsigned char)s[i+1];
        if (c > 5)
        {
            dst[j] = s[i];
            i++; j++;
            continue;
        }
        else if (c == 1)
            d -= 32;
        else if (c == 2)
            d += 192;
        else if (c == 3)
            d += 416;
        else if (c == 4)
            d += 640;
        else if (c == 5)
            d += 864;
        z = (int)(d / 100);
        t = (int)((d-(100*z)) / 10);
        dst[j] = z + '0';
        dst[j+1] = t + '0';
        dst[j+2] = (d % 10) + '0';
        i += 2;
        j += 3;
    }
    dst[j] = '\0';
}

void strcompr(const char *str, char *cpr)
{
    int i, j;
    int lim = strlen(str)-3;

    j =0;
    for (i=0; i<lim; i++, j++)
    {
        if (numcompr(str+i, cpr+j) == 0)
        {
            i += 2;
            j++;
        }
        else
            cpr[j] = str[i];
    }
    lim += 3;
    while (i < lim)
    {
        cpr[j] = str[i];
        i++;
        j++;
    }
    cpr[j] = '\0';
}

void compress(bool rev)
{
    register int i, j;
    char s[MAXSTRLEN+1];
    BUFDEF;

    for (i=0; i<reccnt; i++)
    {
        if (rev)
        {
            RLED(rows[i], buf, MAXSTRLEN);
            numextr(buf, s);
        }
        else
        {
            strcompr(rows[i], buf);
            RLEC(buf, s);
        }
        j = strlen(s);
        if (rows[i] != NULL)
            	free(rows[i]);
        rows[i] = (char *)malloc(j+1);
        strcpy(rows[i], s);
    }
}


void crypt(int n)
{
#define KEYLEN 17
    char key[KEYLEN]="_*tSvDb_EnCrYpT*_";
    char k0, k1, k2;
    int i, j, k, l;

    if (head[0] != '\3')
        	compress(FALSE);
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
        compress(TRUE);
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
    register int i, j, k;
    FILE *fp;
    BUFDEF;
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
            if ((p = strstr(fname, ".tsv")) == NULL)
                strcat(fname, ".tsv");
        }
        else
            fclose(fp);
    }

    i = strlen(fname);
    if ((i > 4) 
    && (fname[i-4]=='.' && fname[i-3]=='c' 
        && fname[i-2]=='s' && fname[i-1]=='v'))
    {
        csep = COLCSEP;
        ssep[0] = COLCSEP;
    }

    if ((fp = fopen(fname, "r")) != NULL)
    {
        buf[0] = '\0';
        fgets(buf, MAXSTRLEN, fp);
        i = strlen(buf);
        for (i--; i>1; i--)
            if (buf[i] > 0x1F)
              	break;
        if (i > 2)
        {
            buf[i+1] = 0x0A;
            buf[i+2] = '\0';
        }
        if ((p = strchr(buf, csep)) == NULL)
        {
            for (i=0; buf[i]!=0; i++)
                if (buf[i] == 0x20)
                {
                    buf[i] = csep;
                    headspac = TRUE;
                }
        }
        if ((p = strstr(buf, ssep)) == NULL)
        {
            sprintf(buf, "ERROR: Can't open file '%s'", fname);
            errormsg(buf);
            fclose(fp);
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
        while (buf[j-1] == '\n')
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
                if (j > MAXFLEN)
                {
                    stru[i][MAXFLEN] = 0;
                    j = MAXFLEN;
                }
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
            stru[cols][len[cols]-1] = ' ';
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
                p = (char *)malloc(j+1);
                if (p == NULL)
                {
                    errormsg("ERROR: Memory full!");
                    ateof = TRUE;
                    break;
                }
                rows[i] = p;
                strcpy(rows[i], buf);
                i++;
                if (i >= MAXROWS)
                {
                    errormsg("ERROR: File too big, truncated!");
                    ateof = TRUE;
                }
            }
            else
                ateof = TRUE;
        }
        reccnt = i;
        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
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
    strcpy(datfname, fname);
    return 0;
}

int getfile(char *fname)
{
    int i, j;
    int k = 0;
    FILE *fp;
    BUFDEF;
    bool ateof = FALSE;
    bool csv_f = FALSE;
    char *p;

    if ((fp = fopen(fname, "r")) != NULL)
    {
        if ((p = strstr(fname, ".csv")) == NULL)
            	csv_f = TRUE;
        i = reccnt;
        while (!ateof)
        {
            buf[0] = '\0';
            fgets(buf, MAXSTRLEN, fp);
            k = strlen(buf);
            if (k)
            {
                if (csv_f)
                {
                    for (j=0; j<k; j++)
                        if (buf[j] == COLCSEP)
                            buf[j] = csep;
                }
                p = (char *)malloc(j+1);
                if (p == NULL)
                {
                    errormsg("ERROR: Memory full!");
                    ateof = TRUE;
                    break;
                }
                modified = TRUE;
                rows[i] = p;
                strcpy(rows[i], buf);
                i++;
                if (i >= MAXROWS)
                {
                    errormsg("ERROR: File too big, truncated!");
                    ateof = TRUE;
                }
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
    strcpy(datfname, fname);
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

#ifdef __MINGW_VERSION
    pause(1);
#else
    sleep(1);
#endif
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
    register int i;
    long int size = 0L;
    FILE *fp;
    char *tmp;
    BUFDEF;

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
    if ((fp = fopen(fname, "r")) != NULL)
    {
        fseek(fp, 0L, SEEK_END);
        size = ftell(fp);
        fclose(fp);
        (void)chmod(fname, S_IRUSR);
        if (size != filesize)
        {
            tmp = tmpnam(NULL);
            strcpy(buf, ".");
            strcat(buf, tmp);
            strcat(buf, "_");
            strcat(buf, fname);
            strcpy(fname, buf);
            strcpy(datfname, buf);
        }
        (void)chmod(fname, S_IWUSR);
    }
    if ((fp = fopen(fname, "w")) != NULL)
    {
        msg(WSTR);
        if (locked && !cry)
        {
            crypt(reccnt);
        }
        if (headspac)
        {
            strcpy(buf, head);
            for (i=0; buf[i]!=0; i++)
                if (buf[i] == csep)
                    buf[i] = 0x20;
            fputs(buf, fp);
        }
        else
            fputs(head, fp);
        for (i=0; i<reccnt; i++)
            fputs(rows[i], fp);
        fflush(fp);
        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
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
    strcpy(datfname, fname);
    modified = FALSE;
#ifdef __MINGW_VERSION
    if (terminable == FALSE)
        	EnableConsoleCloseButton();
    terminable = TRUE;
#endif
    flagmsg();
    return 0;
}

int copyfile(char *fname, char *str, bool head)
{
    int i, j;
    FILE *fp;
    BUFDEF;
    char *p;

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
        while (!feof(fp))
        {
            if (head)
            {
                 printf("%s", buf);
                 head = FALSE;
            }
            buf[0] = '\0';
            fgets(buf, MAXSTRLEN, fp);
            j = strlen(buf);
            if ((i = slre_match(str, buf, j, NULL, 1, 0)) > 0)
                 printf("%s", buf);
        }
        fclose(fp);
    }
    return 0;
}

void clean()
{
    register int i;

    for (i=0; i<=reccnt; i++)
    {
        if (rows[i] != NULL)
           free(rows[i]);
    }
    reccnt = 0;
}

int create(char *fn)
{
    BUFDEF;
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

    j = strlen(head);
    if ((j == 0) || (j > 66))
       strcpy(buf, "?");
    else
    {
       strcpy(buf, head);
       j = strlen(buf);
       for (i=0; i<j; i++)
       {
          if (buf[i] == csep)
              buf[i] = ' ';
          else if (buf[i] == '\n')
               {
                   buf[i] = ' ';
                   buf[i+1] = '\0';
               }
       }
       new = FALSE;
    }
    putmsg("", "To create a new database, enter the field names!", "");
    if (getfname(FALSE, HEADSTR, buf, 68))
    {
        if (strchr(buf, ' ') == NULL)
        {
            errormsg("ERROR: There is not enough field, must have at least two!");
            return -1;
        }
        j = strlen(buf);
        if (j)
        {
            for (i=0; i<j; i++)
            {
                if (buf[i]==' ')
                   buf[i] = csep;
            }
            strcat(buf, "\n");
            strcpy(head, buf);
            i = 0;
            k = 0;
            p = strtok(buf, ssep );
            while(p != NULL )
            {
                strcpy(stru[i], p);
                j = strlen(stru[i]);
                if (j > MAXFLEN)
                {
                    stru[i][MAXFLEN] = 0;
                    j = MAXFLEN;
                }
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
        strcpy(datfname, fn);
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
    int w = MIN(bodywidth(), strlen(head));
    bool firsttab;
    bool quit = FALSE;
    BUFDEF;

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
            x = MIN(strlen(buf), w-x);
            buf[x-1] = '\n';
            buf[x] = '\0';
            firsttab = TRUE;
            for (x=0; x<w; x++)
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
            if (MOUSE_WHEEL_UP && (curr>0))
                curr--;
            else if (MOUSE_WHEEL_DOWN && (curr<k))
                curr++;
            break;
#endif
        case 'Q':
        case 'q':
        case KEY_ESC:
            quit = TRUE;
        }
    }
}

void fltls(void)
{
    int i, j, k;
    int x = reccnt-1;
    int w=bodywidth()-9;
    bool quit = FALSE;
    BUFDEF;

    setcolor(wbody, BODYCOLOR);
    wclear(wstatus);
    wrefresh(wstatus);
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
               if (strlen(buf) > w)
               {
                  buf[w] = '\n';
                  buf[w+1] = '\0';
               }
               for (k=10; k<w; k++)
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
            while (curr < x && flags[curr] != 1)
               curr++;
            break;
        case KEY_HOME:
            curr = 0;
            break;
        case KEY_END:
            clsbody();
            curr = reccnt-1;
            while (curr > 1 && flags[curr] != 1)
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
            if (MOUSE_WHEEL_UP)
            {
                if (curr > 0)
                   curr--;
                while (curr > 0 && flags[curr] != 1)
                   curr--;
            }
            else if (MOUSE_WHEEL_DOWN)
            {
                if (curr < x)
                   curr++;
                while (curr < x && flags[curr] != 1)
                   curr++;
            }
            break;
#endif
        case 'Q':
        case 'q':
        case KEY_ESC:
            quit = TRUE;
        }
    }
}

void disphint(int y)
{
    int i, j;
    int r = bodylen() -1;

    curr = y;
    j = 0;
    for (i=ctop; i<(ctop+r); i++)
    {
        if (i < reccnt)
           displn(i, j+1);
        else
        {
           wmove(wbody, j+1, 0);
           wclrtoeol(wbody);
           wrefresh(wbody);
        }
        j++;
    }
    statusln();
    displn(y, y-ctop+1);
}

void modify(int y)
{
    int i, j, k, l;
    int ret;
    char s[MAXCOLS+1][MAXSTRLEN+1];
    BUFDEF;
    char *p=NULL;
    char *fieldnam[MAXCOLS+1];
    char *fieldbuf[MAXCOLS+1];
    int fieldlim[MAXCOLS+1];
    int flen = 0;
    int maxx;

    if (ro)
        return;
loop:
    p = NULL;
    for (i=0; i<MAXCOLS; i++)
    {
        s[i][0] = 0;
        fieldnam[i] = NULL;
        fieldbuf[i] = NULL;
        fieldlim[i] = 0;
    }
    flen = 0;
    strcpy(buf, rows[y]);
    if (strlen(buf) == 1)
    {
        strcpy(buf, " ");
        for (i=0; i<cols; i++)
        {
            strcat(buf, " ");
            strcat(buf, ssep);
        }
        strcat(buf, " \n");
    }
    else
    {
        k = strlen(buf);
        for (i=0; i<k; i++)
        {
            if ((buf[i] == csep) && (buf[i+1] == csep))
            {
                k++;
                for (j=k; j>i; j--)
                    buf[j] = buf[j-1];
                i++;
                buf[i] = ' ';
            }
        }
    }
    for (i=0; i<=cols; i++)
    {
        flen = MAX(len[i], flen);
    }
    p = strtok(buf, ssep );
    for (i=0; i<=cols; i++)
    {
        if (p != NULL)
        {
            strcpy(s[i], p);
            k = strlen(s[i]);
            l = MAX(len[i], k);
            for (j=0; j<flen; j++)
            {
               if ((s[i][j] == csep)
               ||  (s[i][j] == '\n'))
                  k = j;
               if (j >= k)
                  s[i][j] = ' ';
            }
            for (; j<flen; j++)
               s[i][j] = ' ';
            s[i][flen] = '\0';
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
        fieldlim[i] = len[i];
        l = MAX(len[i], strlen(fieldbuf[i]));
        if (l > flen)
            flen = l;
    }
    flen++;
    getmaxyx(wbody, k, maxx);
    l = (maxx/2)-2;
    if (flen > l)
        flen = l;
    fieldnam[cols+1] = (char *)0;
    fieldbuf[cols+1] = NULL;
    inside = TRUE;
    if ((ret=getstrings(fieldnam, fieldbuf, field, flen, fieldlim)) == KEY_ESC)
    {
        inside = FALSE;
        return;
    }
    else
    {
        pushundo(y);
        modified = TRUE;
        inside = FALSE;
        flagmsg();
    }
    for (i = 0; i <= cols; i++)
    {
        l = len[i];
        k = strlen(fieldbuf[i]);
        if (k < 2)
        {
            strcat(fieldbuf[i], " ");
            k++;
        }
        while (k >= l)
        {
            if (fieldbuf[i][k-1] == ' ')
                k--;
            else
                break;
        }
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
        else
        {
           strcat(buf, ssep);
           strcat(buf, " ");
        }
    }
    strcat(buf, "\n");
    i = strlen(buf);
    if (rows[y] != NULL)
       free(rows[y]);
    rows[y] = (char *)malloc(i+1);
    strcpy(rows[y], buf);
    if (ret == KEY_PPAGE)
    {
        if (y > 0)
        {
            y--;
            if (y < ctop)
                ctop--;
            disphint(y);
            goto loop;
        }
    }
    else if (ret == KEY_NPAGE)
    {
        if (y < (reccnt-1))
        {
            y++;
            if ((y-ctop) >= (bodylen()-1))
                ctop++;
            disphint(y);
            goto loop;
        }
    }
}

int strsplit(const char *str, char *parts[], const char *delimiter)
{
    char *pch;
    int i = 0;
    char *tmp = strdup(str);
    pch = strtok(tmp, delimiter);

    parts[i++] = strdup(pch);
    while (pch)
    {
        pch = strtok(NULL, delimiter);
        if (NULL == pch) break;
        parts[i++] = strdup(pch);
    }
    if (NULL == pch)
        parts[i++] = strdup(" ");
    free(tmp);
    free(pch);
    return i;
}

void strtrim(char *s)
{
    int i = strlen(s);

    if (i == 0)
        return;
    i--;
    while (isspace(s[i]))
    {
        s[i] = '\0';
        i--;
    }
}

void modallf(int y)
{
    int i, j, k, l;
    int ret;
    BUFDEF;
    char *fieldnam[MAXCOLS+1];
    char *fieldbuf[MAXCOLS+1];
    int flen = 0;
    int maxx;

    if (ro)
        return;

loop:
    for (i=0; i<MAXCOLS; i++)
    {
        fieldnam[i] = NULL;
        fieldbuf[i] = NULL;
    }
    flen = 0;
    strcpy(buf, rows[y]);
    i = strlen(buf);
    if (i < 2)
    {
        modify(y);
        return;
    }
    for (i=0,j=0; j<=cols; i++)
    {
        if (buf[i] == csep)
            j++;
        else if (buf[i] == '\r' || buf[i] == '\n' || buf[i] == '\0')
                 break;
    }
    if (j <= cols)
    {
        buf[i] = '\0';
        for (j--; j<=cols; j++)
        {
            strcat(buf, " ");
            strcat(buf, ssep);
        }
        i = strlen(buf);
    }
    if (buf[i-1] == '\r' || buf[i] == '\n')
        buf[i-1] = csep;
    buf[i] = '\0';
    strsplit(buf, fieldbuf, ssep);
    for (i = 0; i <= cols; i++)
    {
        fieldnam[i] = ">";
        if (fieldbuf[i] == NULL)
            fieldbuf[i] = strdup(" ");
    }
    getmaxyx(wbody, k, maxx);
    flen = maxx-(maxx-MAXFLEN)-9;
    fieldnam[cols+1] = (char *)0;
    fieldbuf[cols+1] = NULL;
    inside = TRUE;
    if ((ret = getstrings(fieldnam, fieldbuf, field, flen, NULL)) == KEY_ESC)
    {
        inside = FALSE;
        return;
    }
    else
    {
        pushundo(y);
        modified = TRUE;
        inside = FALSE;
        flagmsg();
    }
    for (i = 0; i <= cols; i++)
    {
        l = len[i];
        k = strlen(fieldbuf[i]);
        if (k < 2)
        {
            strcat(fieldbuf[i], " ");
            k++;
        }
        while (k >= l)
        {
            if (fieldbuf[i][k-1] == ' ')
                k--;
            else
                break;
        }
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
        else
        {
           strcat(buf, ssep);
           strcat(buf, " ");
        }
    }
    strcat(buf, "\n");
    i = strlen(buf);
    if (rows[y] != NULL)
       free(rows[y]);
    rows[y] = (char *)malloc(i+1);
    strcpy(rows[y], buf);
    if (ret == KEY_PPAGE)
    {
        if (y > 0)
        {
            y--;
            if (y < ctop)
                ctop--;
            disphint(y);
            goto loop;
        }
    }
    else if (ret == KEY_NPAGE)
    {
        if (y < (reccnt-1))
        {
            y++;
            if ((y-ctop) >= (bodylen()-1))
                ctop++;
            disphint(y);
            goto loop;
        }
    }
} 

void modfield(int y)
{
    int i, j, k, l;
    BUFDEF;
    char *fieldbuf[MAXCOLS+1];
    int flen = 0;
    int maxx;

    if (ro)
        return;

    strcpy(buf, rows[y]);
    i = strlen(buf);
    if (i < 2)
    {
        buf[0] = ' ';
        i = 2;
    }
    for (i=0,j=0; j<=cols; i++)
    {
        if (buf[i] == csep)
            j++;
        else if (buf[i] == '\r' || buf[i] == '\n' || buf[i] == '\0')
                 break;
    }
    if (j <= cols)
    {
        buf[i] = '\0';
        for (j--; j<=cols; j++)
        {
            strcat(buf, " ");
            strcat(buf, ssep);
        }
        i = strlen(buf);
    }
    if (buf[i-1] == '\r' || buf[i] == '\n')
        buf[i-1] = csep;
    buf[i] = '\0';
    strsplit(buf, fieldbuf, ssep);
    getmaxyx(wbody, k, maxx);
    flen = (maxx)-9;
    fieldbuf[cols+1] = NULL;
    inside = TRUE;
    if (getfname(FALSE, ">", fieldbuf[field], flen) == NULL)
    {
        inside = FALSE;
        return;
    }
    inside = FALSE;
    strtrim(fieldbuf[field]);
    pushundo(y);
    modified = TRUE;
    flagmsg();
    for (i = 0; i <= cols; i++)
    {
        l = len[i];
        k = strlen(fieldbuf[i]);
        if (k < 2)
        {
            strcat(fieldbuf[i], " ");
            k++;
        }
        while (k >= l)
        {
            if (fieldbuf[i][k-1] == ' ')
                k--;
            else
                break;
        }
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
        else
        {
           strcat(buf, ssep);
           strcat(buf, " ");
        }
    }
    strcat(buf, "\n");
    i = strlen(buf);
    if (rows[y] != NULL)
       free(rows[y]);
    rows[y] = (char *)malloc(i+1);
    strcpy(rows[y], buf);
}

void newrec(int y, bool dupl)
{
    register int i;
    char *p=NULL;

    if (ro)
        return;
    if (reccnt >= MAXROWS)
    {
        msg("Too many rows!");
        waitforkey();
        msg(NULL);
        return;
    }
    i = dupl ? strlen(rows[y]) : 2;
    p = (char *)malloc(i);
    if (p == NULL)
    {
        msg("Memory full!");
        waitforkey();
        msg(NULL);
        return;
    }
    reccnt++;
    rows[reccnt+1] = NULL;
    for (i=reccnt; i>y; i--)
    {
        rows[i] = rows[i-1];
    }
    rows[y] = p;
    if (dupl)
        strcpy(rows[y], rows[y+1]);
    else
        strcpy(rows[y], "\n");
    modified = TRUE;
    flagmsg();
}

void purge(int y)
{
    register int i;

    if (ro)
        return;
    if (y<1)
    {
        yesno("First record, edit it!");
        return;
    }
    if ((i=yesno("Delete record! Are You sure? (Y/N):")) == 0)
        return;
    strcpy(undel, rows[y]);
    undelpos = y;
    if (rows[y] != NULL)
       free(rows[y]);
    for (i=y; i<reccnt; i++)
    {
        rows[i] = rows[i+1];
    }
    rows[reccnt] = "\0";
    reccnt--;
    modified = TRUE;
    flagmsg();
}

int substr(char *str1, char *str2)
{
  register int i, j;
  int len1, len2;

  len1 = (int)strlen(str1);
  len2 = (int)strlen(str2);
  for (i = 0; i < len1 - len2 + 1; i++) {
    for (j = 0; j < len2 && ((str2[j] == '?') || (str1[i+j] == str2[j])); j++);
    if (j == len2)
       return (i);
  }
  return (-1);
}

void search(int y, int c)
{
    register int i, j;
    int k=0;
    char cstr[] = "?";
    char s[MAXSTRLEN+1];
    char *p;
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
                p = fstr;
                if (p[0] == '?')
                {
                    p++;
                    for (k=0; s[k]; k++)
                        if (!isalnum(s[k]))
                            strcpy(s+k, s+k+1);
                }
                if (substr(s, p) != -1)
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
                    p = fstr;
                    if (p[0] == '?')
                    {
                        p++;
                        for (k=0; s[k]; k++)
                            if (!isalnum(s[k]))
                                strcpy(s+k, s+k+1);
                    }
                    if (substr(s, p) != -1)
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
            if ((k = slre_match(fstr+(regexcase==0 ? 0 : RXCLEN),
                                rows[i], strlen(rows[i]),
                                &cap, 1, regexcase)) > 0)
            {
                curr = i;
                break;
            }
        }
        if (goregex)
        {
            goregex = FALSE;
            regerr(k, s);
        }
    }
    else if (c == RXBACK)
    {
        for (i=y-1; i>=0; i--)
        {
            if ((k = slre_match(fstr+(regexcase==0 ? 0 : RXCLEN),
                                rows[i], strlen(rows[i]),
                                &cap, 1, regexcase)) > 0)
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
                p = fstr;
                if (p[0] == '?')
                {
                    p++;
                    for (k=0; s[k]; k++)
                        if (!isalnum(s[k]))
                            strcpy(s+k, s+k+1);
                }
                if (substr(s, p) != -1)
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
    setcolor(wstatus, MAINMENUCOLOR);
    mvwaddstr(wstatus, 0, 20, (j!=0) ? "*" : " ");
    setcolor(wstatus, FSTRCOLOR);
    mvwaddstr(wstatus, 0, 21, fstr);
    setcolor(wstatus, STATUSCOLOR);
    wclrtoeol(wstatus);
    wrefresh(wstatus);
}

void searchfield(int y, int x)
{
    register int i, j;
    int k=0;
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
        if ((k = slre_match(fstr+(regexcase==0 ? 0 : RXCLEN), p, strlen(p),
                            &cap, 1, regexcase)) > 0)
        {
            curr = i;
            break;
        }
    }
    if (goregex)
    {
        goregex = FALSE;
        regerr(k, s);
    }
    j = strlen(fstr);
    setcolor(wstatus, MAINMENUCOLOR);
    mvwaddstr(wstatus, 0, 20, (j!=0) ? "*" : " ");
    setcolor(wstatus, FSTRCOLOR);
    mvwaddstr(wstatus, 0, 21, fstr);
    setcolor(wstatus, STATUSCOLOR);
    wclrtoeol(wstatus);
    wrefresh(wstatus);
}

void getfstr(void)
{
    char *fieldname[2];
    char *fieldbuf[2];

    fieldname[0] = "Search:";
    fieldname[1] = 0;
    fieldbuf[0] = regstr;
    fieldbuf[1] = 0;

    if (regstr[0] == '\0')
        	strcpy(regstr, "(?i)");
    getregexp = TRUE;
    getstrings(fieldname, fieldbuf, 0, MAXFLEN+1, NULL);
    goregex = regstr[0] == 0 ? FALSE : TRUE;
    strcpy(fstr, regstr);
    getregexp = FALSE;
    touchwin(wbody);
    wrefresh(wbody);
    regex = TRUE;
    if (strstr(fstr, RXCINS) == fstr)
        regexcase = SLRE_IGNORE_CASE;
    else
        regexcase = 0;
}

void getdupstr(void)
{
    char *fieldname[3];
    char *fieldbuf[3];
    char str1[MAXFLEN] = "";
    char str2[MAXFLEN] = "";

    fieldname[0] = " First:";
    fieldname[1] = "Second:";
    fieldname[2] = 0;
    fieldbuf[0] = str1;
    fieldbuf[1] = str2;
    fieldbuf[2] = 0;

    getregexp = TRUE;
    getstrings(fieldname, fieldbuf, 0, MAXFLEN+1, NULL);
    strcpy(fstr, str1);
    strcat(fstr, ".*");
    strcat(fstr, str2);
    strcat(fstr, "|");
    strcat(fstr, str2);
    strcat(fstr, ".*");
    strcat(fstr, str1);
    if (7 > strlen(fstr))
    {
        fstr[0] = 0;
        goregex = FALSE;
    }
    else
        goregex = TRUE;
    getregexp = FALSE;
    touchwin(wbody);
    wrefresh(wbody);
    regex = TRUE;
    if (strstr(fstr, RXCINS) == fstr)
        regexcase = SLRE_IGNORE_CASE;
    else
        regexcase = 0;
}

void change()
{
    register int i, j, k;
    static char s1[MAXFLEN] = "";
    static char s2[MAXFLEN] = "";
    char *fieldname[3];
    char *fieldbuf[3];
    char s[MAXSTRLEN+1];
    int changes=0;
    int rlen;
    char c;
    char *p;

    if (ro)
        return;

    fieldname[0] = FROMSTR;
    fieldname[1] = TOSTR;
    fieldname[2] = 0;
    if (strlen(fstr) <= MAXFLEN)
        strcpy(s1, fstr);
    fieldbuf[0] = s1;
    fieldbuf[1] = s2;
    fieldbuf[2] = 0;

    getstrings(fieldname, fieldbuf, 0, MAXFLEN+1, NULL);

    rlen = strlen(s1);
    if (rlen == 0)
        return;

    for (i=0; i<reccnt; i++)
    {
        if (s1[0] == '(' && s1[rlen-1] == ')')
        {
            p = slre_replace(s1, rows[i], s2);
            j = strlen(p);
            if (rows[i] != NULL)
               free(rows[i]);
            rows[i] = (char *)malloc(j+1);
            strcpy(rows[i], p);
            changes++;
        }
        else
        while ((k = substr(rows[i], s1)) != -1)
        {
            for (j=0; j<k; j++)
               s[j] = rows[i][j];
            s[k] = '\0';
            strcat(s, s2);
            strcat(s, rows[i]+k+rlen);
            j = strlen(s);
            if (rows[i] != NULL)
               free(rows[i]);
            rows[i] = (char *)malloc(j+1);
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

void schange()
{
    register int i, j;
    int k, l, m, n;
    static char s1[MAXFLEN] = "";
    static char s2[MAXFLEN] = "";
    char *fieldname[3];
    char *fieldbuf[3];
    char s[MAXSTRLEN+1];
    int changes=0;
    int rlen;
    char c=0;
    char *p;
    struct slre_cap cap = { NULL, 0 };
    int last=0;
    bool highlight = TRUE;

    if (ro)
        return;

    fieldname[0] = FROMSTR;
    fieldname[1] = TOSTR;
    fieldname[2] = 0;
    if (strlen(fstr) <= MAXFLEN)
        strcpy(s1, fstr);
    fieldbuf[0] = s1;
    fieldbuf[1] = s2;
    fieldbuf[2] = 0;

    getstrings(fieldname, fieldbuf, 0, MAXFLEN+1, NULL);

    rlen = strlen(s1);
    if (rlen == 0)
        return;

    if (s1[0] == '(' && s1[rlen-1] == ')')
    {
        for (i=0; i<reccnt; i++)
        {
            highlight = TRUE;
            if ((k = slre_match(s1, rows[i], strlen(rows[i]), &cap, 1, 0)) > 0)
            {
                last = (i==0) ? 1 : i;
                displn(i, 1);
                for (j=i; j<(reccnt-i); j++)
                {
                    if (j < reccnt)
                       displn(j, 1+j-i);
                    else
                    {
                       wmove(wbody, (j-i)+1, 0);
                       wclrtoeol(wbody);
                       wrefresh(wbody);
                    }
                }
                m = n = 0;
                for (j=0; j<k; j++)
                {
                    if (rows[i][j] == csep)
                    {
                       m++;
                       n = beg[m];
                       if ((j+1) < n)
                          	highlight = FALSE;
                    }
                }
                if (highlight)
                {
                    setcolor(wbody, MARKCOLOR);
                    l = rlen-2;
                    strcpy(s, rows[i]+(k-l));
                    s[l] = '\0';
                    mvwaddstr(wbody, 1, k-l, s);
                }
                setcolor(wbody, BODYCOLOR);
                wrefresh(wbody);
                msg("Change/Next/Prev/Quit? (C/N/P/Q):");
                do {
                    c = toupper(waitforkey());
                } while ((p = strchr("CNPQ", c)) == NULL);
                msg(NULL);
                if (c == 'N')
                    continue;
                else if (c == 'Q')
                    break;
                else if (c == 'P')
                     {
                         for (i=last-1; i>0; i--)
                             if ((j = slre_match(s1, rows[i], 
                                  strlen(rows[i]), &cap, 1, 0)) > 0)
                                 break;
                         i--;
                         continue;
                     }
                else if (c == 'C')
                     {
                         p = slre_replace(s1, rows[i], s2);
                         j = strlen(p);
                         if (rows[i] != NULL)
                            free(rows[i]);
                         rows[i] = (char *)malloc(j+1);
                         strcpy(rows[i], p);
                         changes++;
                     }
            }
        }
    }
    else
    {
        for (i=0; i<reccnt; i++)
        {
            while (TRUE)
            {
                highlight = TRUE;
                k = substr(rows[i], s1);
                l = substr(rows[i], s2);
                if ((k+l) == -2)
                    break;
                last = (i==0) ? 1 : i;
                displn(i, 1);
                for (j=i; j<(reccnt-i); j++)
                {
                    if (j < reccnt)
                       displn(j, 1+j-i);
                    else
                    {
                       wmove(wbody, (j-i)+1, 0);
                       wclrtoeol(wbody);
                       wrefresh(wbody);
                    }
                }
                m = n = 0;
                for (j=0; j<k; j++)
                {
                    if (rows[i][j] == csep)
                    {
                       m++;
                       n = beg[m];
                       if ((j+1) < n)
                          	highlight = FALSE;
                    }
                }
                m = n = 0;
                for (j=0; j<l; j++)
                {
                    if (rows[i][j] == csep)
                    {
                       m++;
                       n = beg[m];
                       if ((j+1) < n)
                          	highlight = FALSE;
                    }
                }
                if (highlight)
                {
                    setcolor(wbody, MARKCOLOR);
                    if (l == -1)
                    {
                        strcpy(s, rows[i]+k);
                        s[rlen] = '\0';
                        mvwaddstr(wbody, 1, k, s);
                    }
                    else
                    {
                        strcpy(s, rows[i]+l);
                        s[strlen(s2)] = '\0';
                        mvwaddstr(wbody, 1, l, s);
                    }
                }
                setcolor(wbody, BODYCOLOR);
                wrefresh(wbody);
                msg("Change/Next/Prev/Quit? (C/N/P/Q):");
                do {
                    c = toupper(waitforkey());
                } while ((p = strchr("CNPQ", c)) == NULL);
                msg(NULL);
                if ((c == 'N') || (c == 'Q'))
                    break;
                else if (c == 'P')
                     {
                         for (i=last-1; i>0; i--)
                             if (((j = substr(rows[i], s1)) != -1)
                             ||  ((j = substr(rows[i], s2)) != -1))
                                 break;
                         continue;
                     }
                if (l == -1)
                {
                    for (j=0; j<k; j++)
                       s[j] = rows[i][j];
                    s[k] = '\0';
                    strcat(s, s2);
                    strcat(s, rows[i]+k+strlen(s1));
                    changes++;
                }
                else
                {
                    for (j=0; j<l; j++)
                       s[j] = rows[i][j];
                    s[l] = '\0';
                    strcat(s, s1);
                    strcat(s, rows[i]+l+strlen(s2));
                    if (changes > 0)
                        changes--;
                    l = -1;
                }
                j = strlen(s);
                if (rows[i] != NULL)
                   free(rows[i]);
                rows[i] = (char *)malloc(j+1);
                strcpy(rows[i], s);
                i = last;
            }
            if (c == 'Q')
                break;
        }
    }
    if (changes)
    {
        displn(last, 1);
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


void copy()
{
    register int i, j;
    char c;

    i = 0;
    j = 0;
    if (field > 0)
    {
        while ((c = rows[curr][i]) != '\0')
        {
            i++;
            if (c == csep)
               j++;
            if (j == field)
               break;
        }
    }

    if (rows[curr][i] == ' ')
    {
        c = rows[curr][i+1];
        if ((c == csep) || (c == '\0') || (c == '\n') || (c == '\r'))
            return;
    }

    j = 0;
    while (1)
    {
        c = rows[curr][i];
        if ((c == csep) || (c == '\0') || (c == '\n') || (c == '\r'))
            break;
        i++;
        clp[j] = c;
        j++;
    }
    clp[j] = '\0';
}

void paste()
{
    int i, j, k, l;
    char c;
    BUFDEF;

    if (ro)
        return;

    if (clp[0] == '\0')
        return;

    strcpy(buf, rows[curr]);
    i = 0;
    j = 0;
    if (field > 0)
    {
        while ((c = buf[i]) != '\0')
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
        c = buf[i];
        if ((c == csep) || (c == '\0') || (c == '\n') || (c == '\r'))
        {
            l = strlen(buf);
            for (k=l; k>i; k--)
                buf[k] = buf[k-1];
        }
        c = clp[j];
        if (c == '\0')
        {
            buf[i] = ' ';
            break;
        }
        buf[i] = c;
        i++;
        j++;
        if (j >= (len[field]-1))
            break;
    }
    i = strlen(buf);
    if (rows[curr] != NULL)
       free(rows[curr]);
    rows[curr] = (char *)malloc(i+1);
    strcpy(rows[curr], buf);
    modified = TRUE;
    flagmsg();
}    

int selectfield(int n)
{
    WINDOW *wmsg;
    register int i, j;
    int k =0;
    int sx, sy;
    bool exit=FALSE;
    
    for (i=0; i<=n; i++)
    {
        j = len[i];
        if (j > k)
            k = j;
    }
    sx = (bodywidth()-k)/6;
    sy = (bodylen()-(n+2))/2;
    wmsg = mvwinputbox(wbody, sy, sx, n+3, k+2);
    sy += 2;

    i = field;

    while (!exit)
    {
        setcolor(wmsg, SUBMENUCOLOR);
        for (j=0; j<=n; j++)
            mvwaddstr(wmsg, j+1, 1, stru[j]);
        setcolor(wmsg, SUBMENUREVCOLOR);
        mvwaddstr(wmsg, i+1, 1, stru[i]);
        wrefresh(wmsg);
        j = i;
        switch (waitforkey())
        {
        case KEY_HOME:
        case KEY_PPAGE:
            i = 0;
            break;
        case KEY_UP:
            i = i>0 ? i-1 : n;
            break;
        case KEY_END:
        case KEY_NPAGE:
            i = n;
            break;
        case KEY_DOWN:
            i = i<n ? i+1 : 0;
            break;
        case 'Q':
        case 'q':
        case KEY_ESC:
            i = -1;
/*        case KEY_ENTER:*/
        case '\n':
            exit = TRUE;
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
                if (((MOUSE_Y_POS < sy) || (MOUSE_Y_POS > (sy+n+2)))
                || ((MOUSE_X_POS < sx) || (MOUSE_X_POS >= (sx+k))))
                {
                    i = -1;
                    exit = TRUE;
                }
                else
                {
                    if (MOUSE_Y_POS == sy)
                        i = i>0 ? i-1 : 0;
                    else if (MOUSE_Y_POS == (sy+n+2))
                        i = i<n ? i+1 : n;
                    else
                    {
                        i = MOUSE_Y_POS-(sy+1);
                        if (i == j)
                            exit = TRUE;
                    }
                }
            }
            if (MOUSE_WHEEL_UP)
                i = i>0 ? i-1 : n;
            else if (MOUSE_WHEEL_DOWN)
                i = i<n ? i+1 : 0;
            break;
#endif
        }
    }

    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
    return i;
}

void calc(bool repl)
{
    double sx, dx;

    clp[0] = '\0';
    copy();
    if (clp[0] == '\0')
        return;
    if (repl)
    {
        if (ro)
            return;
        sx = atof(clp);
        calcexp(clp);
        if (calcerr)
            return;
        dx = atof(clp);
        if (dx != sx)
            paste();
    }
    else
    {
        calcExpression(clp);
        putmsg(calcerr ? "ERROR: " : "", clp, "");
    }
}

void evalall()
{
    int i;

    if (ro)
        return;

    if ((i=yesno("Evaluate whole column! Are You sure? (Y/N):")) == 0)
        return;

    i = curr;
    for (curr=0; curr<reccnt; curr++)
    {
        calc(TRUE);
    }
    curr = i;
}

void calcall()
{
    int i;

    if (ro)
        return;

    i = selectfield(cols);
    if (i != -1)
    {
        field = i;
        evalall();
    }
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
    register int i, j;
    FILE *fp;
    char *p;
    char tmpfname[MAXSTRLEN];
    BUFDEF;

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
    strcat(tmpfname, ".tmp");
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
        def_prog_mode();
        endwin();
        if (j == 0)
            return;
/*        execlp("tsvdb", tmpfname, (char *)0);*/
        strcpy(buf, progname);
        strcat(buf, " ");
        strcat(buf, tmpfname);
        system(buf);
        reset_prog_mode();
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

void tsv_filter(void)
{
    int i, j;

    getfstr();
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
       filtered = TRUE;
       selected();
    }
    else
       filtered = FALSE;
}

void dosort(void)
{
    if (ro && !locked)
        return;
    if (yesno("Sort database? (Y/N):") == 0)
        return;
    if (yesno("Reverse order? (Y/N):") == 0)
        sort(reccnt);
    else
        sort_back(reccnt);
    if (!locked)
        modified = TRUE;
    flagmsg();
    redraw();
}

int putmsg(char *beg, char *str, char *end)
{
    WINDOW *wmsg;
    int i;
    char s[MAXSTRLEN];

    strcpy(s, beg);
    strcat(s, str);
    strcat(s, end);
        
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


void reorder(int y, bool left)
{
    register int i, j, k;
    BUFDEF;
    char tmpstr[MAXCOLS+1][MAXSTRLEN+1];
    char *p;
    j = left ? field-1 : field+1;

    for (i=0; i<=cols; i++)
        tmpstr[i][0] = '\0';
    strcpy(buf, rows[y]);
    k = strlen(buf);
    for (i=0; i<k; i++)
    {
        if (buf[i] == '\n' || buf[i] == '\r')
        {
            buf[i] = ' ';
        }
        if ((buf[i] == csep) && (buf[i+1] == csep))
        {
            k++;
            for (j=k; j>i; j--)
                buf[j] = buf[j-1];
            i++;
            buf[i] = ' ';
        }
    }
    i = 0;
    p = strtok(buf, ssep);
    while(p != NULL )
    {
        strcpy(tmpstr[i], p);
        i++;
        if (i > cols)
           break;
        p = strtok(NULL, ssep );
    }
    for (i=0; i<=cols; i++)
    {
        k = strlen(tmpstr[i]);
        if (k == 0)
            strcpy(tmpstr[i], " ");
    }
    buf[0] = '\0';
    for (i=0; i<=cols; i++)
    {
        k = strlen(tmpstr[i]);
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

void reordall(bool left)
{
    register int i, j, k;
    char *p;
    BUFDEF;

    if (ro || safe)
        return;

    if (yesno("Reorder all fields? (Y/N):") == 0)
        return;

    msg(WSTR);
    for (i=0; i<reccnt; i++)
        reorder(i, left);

    for (i=0; i<=cols; i++)
    {
        j = strlen(stru[i])-1;
        if (stru[i][j] == ' ')
            stru[i][j] = '\0';
    }
    j = (left) ? -1 : 1;
    strcpy(buf, stru[field+j]);
    strcpy(stru[field+j], stru[field]);
    strcpy(stru[field], buf);
    strcpy(head, stru[0]);
    for (j=1; j<=cols; j++)
    {
        strcat(head, ssep);
        strcat(head, stru[j]);
    }
    strcpy(buf, head);
    strcat(head, "\n");
    i = 0;
    k = 0;
    p = strtok(buf, ssep);
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
        p = strtok(NULL, ssep);
    }
    stru[cols][len[cols]-1] = '\0';
    msg(NULL);
}

void fieldcase(bool up, bool whole)
{
    char s[MAXSTRLEN+1];
    register int i, j;

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

    itoa(curr+1, s, 10);
    getstrings(fieldname, fieldbuf, 0, 7, NULL);
    i = atoi(s);
    if ((i > 0) && (i <= reccnt))
        curr = i-1;
}

void dosortby(void)
{
    register int i, j;

    if (ro && !locked)
        return;

    i = selectfield(cols);
    if (i != -1)
    {
        putmsg("Sorted by ", stru[i], " field.");
        sortpos = i;
        hunsort = FALSE;
        if (numsort == FALSE)
        {
            if (yesno("Hungarian abc? (Y/N):") != 0)
                hunsort = TRUE;
        }
        if (yesno("Reverse order? (Y/N):") == 0)
            sort(reccnt);
        else
            sort_back(reccnt);
        sortpos = 0;
        if (!locked)
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

void dosortnum(void)
{
    numsort = TRUE;
    dosortby();
    numsort = FALSE;
}


void align(int n, int y, int m)
{
    register int i, j;
    int k, l;
    int beg, dis;
    int from, to;
    BUFDEF;

    if (y == -1)
    {
        from = 0;
        to = reccnt;
    }
    else
    {
        from = y;
        to = y+1;
        modified = TRUE;
        flagmsg();
    }

    for (i=from; i<to; i++)
    {
        strcpy(buf, rows[i]);
        l = strlen(buf);
        k = 0;
        for (j=0; j<n; j++)
        {
            while (k < l)
            {
                if (buf[k] == csep)
                    break;
                k++;
            }
            k++;
        }
        beg = k;
        if (buf[beg] == '"')
            beg++;
        dis = 0;
        while (k < l)
        {
            dis++;
            if (buf[k] == csep)
                break;
            k++;
        }
        dis = len[n]-dis;
        switch (m)
        {
        case 1:
            dis = 0;
            while (buf[beg+dis] == ' ')
                dis++;
            if (dis == 0)
                continue;
            for (j=beg; j<=(l-dis); j++)
            {
                buf[j] = buf[j+dis];
            }
            break;
        case 3:
            if (dis <= 0)
                continue;
            for (j=l+dis; j>=beg; j--)
            {
                if (j >= (beg+dis))
                    buf[j] = buf[j-dis];
                else
                    buf[j] = ' ';
            }
            break;
        case 2:
            if (dis <= 0)
                continue;
            dis = (int)(dis/2);
            for (j=l+dis; j>=beg; j--)
            {
                if (j >= (beg+dis))
                    buf[j] = buf[j-dis];
                else
                    buf[j] = ' ';
            }
            break;
        }
        j = strlen(buf);
        if (rows[i] != NULL)
           free(rows[i]);
        rows[i] = (char *)malloc(j+1);
        strcpy(rows[i], buf);
    }
}

void doindent(void)
{
    int i, j;

    if (ro)
        return;

    i = selectfield(cols);
    if (i != -1)
    {
#ifdef __MINGW_VERSION
        pause(1);
#else
        sleep(1);
#endif
        do {
            j = putmsg("", "Align left, center or right? (1/2/3):", "");
            if (j == KEY_ESC)
                	return;
            j -= '0';
        } while (j<1 || j>3);
        if (j == 2)
            align(i, -1, 1);
        align(i, -1, j);
        putmsg("Adjusted ", stru[i],
               (j==1) ? "left" : (j==3) ? "right" : "center");
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
    register int i, j, k;
    int l;
    double n;
    double x[MAXCOLS];
    char s[MAXCOLS][MAXSTRLEN+1];
    BUFDEF;
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
        l = strlen(buf);
        for (j=0; j<l; j++)
        {
            if ((buf[j] == csep) && (buf[j+1] == csep))
            {
                l++;
                for (k=l; k>j; k--)
                    buf[k] = buf[k-1];
                j++;
                buf[j] = ' ';
            }
        }
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
            if (bp != NULL)
            {
                n = calcExpression(s[j]);
                if (calcerr)
                    n = 0.0;
            }
            x[j] += n;
        }
    }
    for (i = 0; i <= cols; i++)
    {
        sprintf(s[i], "%f %c", x[i], '\0');
        fieldnam[i] = stru[i];
        fieldbuf[i] = s[i];
    }
    fieldnam[cols+1] = (char *)0;
    fieldbuf[cols+1] = s[cols+1];
    j = getstrings(fieldnam, fieldbuf, 0, flen, NULL);
}

void limit(bool set)
{
    register int i, j, k;
    int l;
    bool limited = (rows[0][0] == '"');
    BUFDEF;

    for (i=0; i<reccnt; i++)
    {
        if (rows[i] != NULL)
        {
            strcpy(buf, rows[i]);
            l = strlen(buf);
            if (buf[0] == '"')
            {
                for (k=0; k<=l; k++)
                    buf[k] = buf[k+1];
            }
            for (j=1; j<l; j++)
            {
                if (buf[j] == '"')
                {
                    if ((buf[j-1] == csep)
                    || (buf[j+1] == csep)
                    || (buf[j+1] == '\n'))
                        for (k=j; k<l; k++)
                            buf[k] = buf[k+1];
                    else
                        limited = FALSE;
                }
            }
            if (set)
            {
                l = strlen(buf);
                if (buf[0] != '"')
                {
                    l++;
                    for (k=l; k>0; k--)
                        buf[k] = buf[k-1];
                    buf[0] = '"';
                }
                for (j=2; j<l; j++)
                {
                    if ((buf[j] == csep)
                    || (buf[j] == '\n'))
                    {
                        l++;
                        for (k=l; k>=j; k--)
                            buf[k] = buf[k-1];
                        buf[j] = '"';
                        j++;
                        if (buf[j] != '\n')
                        {
                            l++;
                            j++;
                            for (k=l; k>=j; k--)
                                buf[k] = buf[k-1];
                            buf[j] = '"';
                        }
                        else
                        {
                            buf[j+1] = '\0';
                        }
                    }
                }
            }
            l = strlen(buf);
            if (rows[i] != NULL)
                free(rows[i]);
            rows[i] = (char *)malloc(l+1);
            strcpy(rows[i], buf);
        }
    }
    if (set)
        for (j=0; j<=cols; j++)
            len[j] += 2;
    if (limited)
        for (j=0; j<=cols; j++)
            len[j] -= 2;
    modified = TRUE;
    redraw();
    flagmsg();
}

void delimit(void)
{
    if (ro || crypted)
        return;
    if (yesno("Set fields delimiter? (Y/N):") == 0)
        return;
    limit(TRUE);
}

void unlimit(void)
{
    if (ro || crypted)
        return;
    if (yesno("Purge all delimiters? (Y/N):") == 0)
        return;
    limit(FALSE);
}

void dosep(void)
{
    register int i, j;
    char c = csep;
    char buf[10] = "";

    if (ro || crypted)
        return;
    if (yesno("Set fields separator? (Y/N):") == 0)
        return;
    
    switch (c)
    {
      case SCLCSEP:
        csep = TABCSEP;
        ssep[0] = TABCSEP;
        break;
      case TABCSEP:
        csep = COLCSEP;
        ssep[0] = COLCSEP;
        break;
      case COLCSEP:
        csep = SCLCSEP;
        ssep[0] = SCLCSEP;
        break;
      default:
        break;
    }

    for (j=0; head[j]; j++)
        if (head[j] == c)
            head[j] = csep;

    for (i=0; i<reccnt; i++)
    {
        if (rows[i] != NULL)
        {
            for (j=0; rows[i][j]; j++)
                if (rows[i][j] == c)
                    rows[i][j] = csep;
        }
    }

    switch (csep)
    {
        case '\t':
            strcpy(buf, "TAB");
            break;
        case ';':
            strcpy(buf, "SEMI");
        case ',':
            strcat(buf, "COLON");
    }
    putmsg("Field separator is ", buf, "");

    modified = TRUE;
    redraw();
    flagmsg();
}


void xchange(bool next)
{
    char *p;

    if (!ro)
    {
        p = rows[curr];
        if (next)
        {
            if (curr < (reccnt-1))
            {
                rows[curr] = rows[curr+1];
                curr++;
                rows[curr] = p;
                modified = TRUE;
                flagmsg();
            }
        }
        else
        {
            if ((curr > 0) && (curr < reccnt))
            {
                rows[curr] = rows[curr-1];
                curr--;
                rows[curr] = p;
                modified = TRUE;
                flagmsg();
            }
        }
    }
}


void insfield(void)
{
    register int i, j;
    int k, l;
    char fldname[MAXFLEN+1] = "";
    BUFDEF;

    if ((ro) || (safe) || (cols == 16))
        return;

    if ((i = strlen(head)) > 66)
        return;

    if (getfname(FALSE, "Enter new field name:", fldname, MAXFLEN))
    {
        if (fldname[0] == '\0')
            return;
        strcat(fldname, TABSSEP);
        l = strlen(fldname);
        i = beg[field];
        strcpy(buf, head+i);
        strcpy(head+i, fldname);
        strcat(head, buf);
        cols++;
        for (i=cols; i>field; i--)
        {
            strcpy(stru[i], stru[i-1]);
            beg[i] = beg[i-1]+l;
            len[i] = len[i-1];
        }
        strcpy(stru[field], fldname);
        len[field] = l;
        for (i=0; i<reccnt; i++)
        {
            if (field == 0)
            {
                strcpy(buf, "~");
                strcat(buf, ssep);
                strcat(buf, rows[i]);
            }
            else
            {
                strcpy(buf, rows[i]);
                l = strlen(buf);
                for (j=0,k=1; j<l; j++)
                {
                    if (buf[j] == csep)
                        k++;
                    if (k > field)
                        break;
                }
                if ((k < field) || (j >= l))
                    continue;
                k = j+1;
                for (j=l; j>=k; j--)
                {
                    buf[j+2] = buf[j];
                }
                buf[k] = '~';
                buf[k+1] = csep;
            }
            if (rows[i] != NULL)
                free(rows[i]);
            l = strlen(buf);
            rows[i] = (char *)malloc(l+1);
            strcpy(rows[i], buf);
        }
        modified = TRUE;
        redraw();
        flagmsg();
    }
    curs_set(1);
}

void delfield(void)
{
    register int i, j;
    int k, l;
    BUFDEF;

    if (ro || safe) 
        return;

    if (cols > 0)
    {
        if ((i=yesno("Delete field! Are You sure? (Y/N):")) == 0)
            return;
        l = len[field];
        i = beg[field];
        strcpy(buf, head+i+l);
        strcpy(head+i, buf);
        cols--;
        for (i=field; i<=cols; i++)
        {
            strcpy(stru[i], stru[i+1]);
            beg[i] = beg[i+1]-l;
            len[i] = len[i+1];
        }
        for (i=0; i<reccnt; i++)
        {
            strcpy(buf, rows[i]);
            l = strlen(buf);
            if (field > cols)
            {
                for (j=l; j>0; j--)
                {
                    if (buf[j] == csep)
                        break;
                }
            }
            else
            {
                for (j=0,k=0; j<l; j++)
                {
                    if (buf[j] == csep)
                        k++;
                    if (k > field)
                        break;
                }
                if ((k < field) || (j >= l))
                    continue;
            }
            k = j+1;
            for (j--; j>0; j--)
            {
                if (buf[j] == csep)
                    break;
            }
            strcpy(buf+j+1, buf+k);
            if (field == cols)
            {
                strcat(buf, "\n");
            }
            if (rows[i] != NULL)
                free(rows[i]);
            l = strlen(buf);
            rows[i] = (char *)malloc(l+1);
            strcpy(rows[i], buf);
        }
        if (field > cols)
        {
            strcat(head, "\n");
            field--;
        }
        modified = TRUE;
        redraw();
        flagmsg();
    }
}

void count(bool column)
{
    register int i, j;
    int k = 0;
    char *p;
    struct slre_cap cap = { NULL, 0 };
    char s[MAXSTRLEN+1];

    getregexp = TRUE;
    if (getfname(FALSE, "Count for:", regstr, MAXFLEN))
    {
        if (column)
        {
            k = 0;
            for (i=0; i<reccnt; i++)
            {
                strcpy(s, rows[i]);
                p = s;
                if (field == 0)
                {
                    for (j=0; !(s[j]==csep || s[j]=='\0'); j++);
                    s[j] = '\0';
                }
                else
                {
                    for (j=0; j<field; j++)
                    {
                        while (!(p[0]==csep || p[0]=='\0')) p++;
                        p++;
                    }
                    for (j=0; !(p[j]==csep || p[j]=='\0'); j++);
                    p[j] = '\0';
                }
                if ((j = slre_match(regstr, p, strlen(p), &cap, 1, 0)) > 0)
                    	k++;
            }
        }
        else
        {
            k = 0;
            for (i=0; i<reccnt; i++)
            {
                if ((j = slre_match(regstr, rows[i], strlen(rows[i]),
                                    &cap, 1, 0)) > 0)
                    	k++;
            }
        }
        strcpy(s, "\" is ");
        strcat(s, itoa(k, s+6, 10));
        strcat(s, " ");
        i = strlen(regstr)+strlen(s)+len[field]+26;
        if (i > bodywidth())
            	column = FALSE;
        if (column)
        {
            strcat(s, "in ");
            strcat(s, stru[field]);
        }
        putmsg(" Occurence of the \"", regstr, s);
#ifdef __MINGW_VERSION
        pause(1);
#else
        sleep(1);
#endif
    }
    getregexp = FALSE;
}

void gominmax(bool ismax)
{
    register int i, j, k;
    int l;
    unsigned int width;
    int x = curr;
    int y = curr;
    double n;
    double minmax;
    char s[MAXSTRLEN+1];
    BUFDEF;
    char *p=NULL;
    char *bp;

    minmax = ismax ? -INFINITY : INFINITY;
    width = ismax ? 0 : MAXSTRLEN;
    for (i=0; i<reccnt; i++)
    {
        strcpy(buf, rows[i]);
        l = strlen(buf);
        for (j=0; j<l; j++)
        {
            if ((buf[j] == csep) && (buf[j+1] == csep))
            {
                l++;
                for (k=l; k>j; k--)
                    buf[k] = buf[k-1];
                j++;
                buf[j] = ' ';
            }
        }
        p = strtok(buf, ssep );
        for (j=0; j<=field; j++)
        {
            if (p != NULL)
            {
                strcpy(s, p);
                p = strtok( 0, ssep );
            }
            else
                strcpy(s, " ");
        }
        k = width;
        l = strlen(s);
        width = ismax ? MAX(width, l) : MIN(width, l);
        if (width != k)
        {
            y = i;
        }
        n = strtod(s, &bp);
        if (bp != NULL)
        {
            n = calcExpression(s);
            if (calcerr)
                n = INFINITY;
        }
        if (!isnan(n) && (n != INFINITY))
        {
            if (ismax)
            {
                if (isgreater(n, minmax))
                {
                    minmax = n;
                    x = i;
                }
           
            }
            else
            {
                if (isless(n, minmax))
                {
                    minmax = n;
                    x = i;
                }
            }
        }
    }
    curr = (fabs(minmax) == INFINITY) ? y : x;
}

void golen(bool ismax)
{
    register int i, j, k;
    unsigned int width;
    int x = curr;

    width = ismax ? 0 : MAXSTRLEN;
    for (i=0; i<reccnt; i++)
    {
        j = strlen(rows[i]);
        k = width;
        width = ismax ? MAX(width, j) : MIN(width, j);
        if (width != k)
        {
            x = i;
        }
    }
    curr = x;
}


static int found = 0;

void findequal(bool column)
{
    register int i;
    BUFDEF;
    char *fields0[MAXCOLS+1];
    char *fields1[MAXCOLS+1];

    if (found > curr)
        curr++;
    for (i=curr; i<(reccnt-1); i++)
    {
        if (column)
        {
            strcpy(buf, rows[i]);
            strsplit(buf, fields0, ssep);
            strcpy(buf, rows[i+1]);
            strsplit(buf, fields1, ssep);
            if (strcmp(fields0[field], fields1[field]) == 0)
            {
                curr = i;
                found = (found == i) ? i+1 : i;
                return;
            }
        }
        else if (strcmp(rows[i], rows[i+1]) == 0)
        {
            curr = i;
            found = (found == i) ? i+1 : i;
            return;
        }
    }
    found = 0;
}


int topset(int top, int y)
{
    register int i;

    if (curr > 0)
    {
        for (i=0; i<curr; i++)
        {
            if ((i-top) >= y)
               top++;
        }
    }
    return top;
}

void modstru(void)
{
    int i, j;
    char c1[MAXCOLS][4];
    char c2[MAXCOLS][MAXFLEN];
    char st[MAXCOLS+1][MAXSTRLEN];
    char *fieldnam[MAXCOLS+1];
    char *fieldbuf[MAXCOLS+1];
    int fieldlim[MAXCOLS+1];
    BUFDEF;
    int flen = 0;
    int maxx;

    if (ro || safe || (strlen(head) < 9))
        return;

    if (yesno("Modify structure? (Y/N):") == 0)
        return;

    for (i=0; i<=cols; i++)
    {
        flen = MAX(len[i], flen);
    }

    for (i=0; i<MAXCOLS; i++)
    {
        strcpy(st[i], stru[i]);
        if (i < 9)
        {
            c1[i][0] = ' ';
            c1[i][1] = '\0';
        }
        else
            c1[i][0] = '\0';
        strcat(c1[i], itoa(i+1, buf, 10));
        strcat(c1[i], ":");
        fieldnam[i] = c1[i];
        if (i <= cols)
        {
            fieldbuf[i] = stru[i];
            fieldlim[i] = len[i];
            j = MAX(len[i], strlen(fieldbuf[i]));
            if (j > flen)
                flen = j;
        }
        else
        {
            strcpy(c2[i], " ");
            fieldbuf[i] = c2[i];
            fieldlim[i] = MAXFLEN;
        }
    }
    flen++;
    getmaxyx(wbody, j, maxx);
    j = (maxx/2)-2;
    if (flen > j)
        flen = j;
    fieldnam[MAXCOLS] = NULL;
    fieldbuf[MAXCOLS] = NULL;
    strcpy(buf, head);

    if (getstrings(fieldnam, fieldbuf, 0, flen, fieldlim) != KEY_ESC)
    {
        if (fieldbuf[1][0] == '\0')
        {
            putmsg("","Must be at least two fields!","");
            strcpy(head, buf);
            for (i=0; i<MAXCOLS; i++)
                	strcpy(stru[i], st[i]);
            return;
        }
        strtrim(fieldbuf[0]);
        strcpy(head, fieldbuf[0]);
        for(i=1; i<MAXCOLS; i++)
        {
            strtrim(fieldbuf[i]);
            if (0 == strlen(fieldbuf[i]))
                	break;
            strcat(head, ssep);
            strcat(head, fieldbuf[i]);
        }
        strcat(head, "\n");
        headspac = FALSE;
        if (savefile(datfname, 1) == 0)
        {
            if (loadfile(datfname) == 0)
            {
                redraw();
                flagmsg();
            }
        }
        else
        {
            strcpy(head, buf);
            for (i=0; i<MAXCOLS; i++)
                	strcpy(stru[i], st[i]);
        }
    }
}

//#if !defined(__unix) || defined(__DJGPP__)
void lat2(char *c, int x)
{
    int i, j;
    char *p;
    char c0[] =
    "\xA0\x82\xA1\xA2\x94\x8B\xA3\x81\xB5\x90\xD6\xE0\x99\x8A\xE9\x9A\xEB";
    char c1[] =
    "\xE1\xE9\xED\xF3\xF6\xF5\xFA\xFC\xC1\xC9\xCD\xD3\xD6\xD5\xDA\xDC\xDB";
   
    if (ro) 
        return;
   
    for (j = strlen(c); j > 0; j--)
    {
        if (x == 0)
            p = strchr(c0, c[j]);
        else
            p = strchr(c1, c[j]);
        if (p != NULL)
        {
            modified = TRUE;
            if (x == 0)
            {
               i = p - c0;
               c[j] = c1[i];
            }
            else
            {
               i = p - c1;
               c[j] = c0[i];
            }
        }
    }
}

void docode(void)
{
    int i, j;

    if (ro || safe) 
        return;

    j = (yesno("To Latin-2 ? (Y/N):") == 0) ? 0 : 1;
    for (i=0; i<(reccnt); i++)
    {
        lat2(rows[i], j);
    }
    redraw();
    flagmsg();
}
//#endif


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
    bool unget= FALSE;
    int c;

    curr = (curr <= reccnt) ? curr : 0;
    ctop = topset(curr, r);
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
            else
            {
               wmove(wbody, j+1, 0);
               wclrtoeol(wbody);
               wrefresh(wbody);
            }
            j++;
        }
        statusln();
        displn(curr, curr-ctop+1);
        if (unget == FALSE)
            	c = waitforkey();
        else
            	unget = FALSE;
        switch (c)
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
               j = beg[field] + len[field];
               if (j>i)
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
                  ctop = b+1;
            }
            break;
        case KEY_HOME:
            curr = 0;
            ctop = 0;
            fchr = fstr[0];
            fstr[0] = '\0';
            regex = FALSE;
            memset(flags, 0, MAXROWS);
            wmove(wstatus, 0, 20);
            wclrtoeol(wstatus);
            wrefresh(wstatus);
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
        case CTRL_E:
        case CTL_ENTER:
        case CTL_PADENTER:
            if (curr < reccnt)
            {
               modfield(curr);
               curs_set(1);
            }
            break;
#ifndef __MINGW_VERSION
        case ALT_ENTER:
#endif
        case ALT_E:
        case ALT_PADENTER:
            if (curr < reccnt)
            {
               modallf(curr);
               curs_set(1);
            }
            break;
        case CTL_PADPLUS:
        case CTL_INS:
            newrec(curr, FALSE);
            b = reccnt-bodylen();
            break;
        case ALT_PADPLUS:
        case ALT_INS:
            newrec(curr, TRUE);
            b = reccnt-bodylen();
            break;
        case CTL_DEL:
            if (curr < reccnt)
            {
               purge(curr);
               b = reccnt-bodylen();
            }
            break;
        case ALT_T:
        case ALT_DEL:
            if (undelpos != 0)
            {
               curr = undelpos;
               newrec(curr, FALSE);
               i = strlen(undel);
               if (rows[curr] != NULL)
                   	free(rows[curr]);
               rows[curr] = (char *)malloc(i+1);
               strcpy(rows[curr], undel);
               undelpos = 0;
               b = reccnt-bodylen();
               if ((curr < ctop) || (curr >= (ctop+r)))
                   	ctop = topset(curr, r);
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
        case KEY_SDC:
            if (strlen(fstr))
            {
               strcpy(fstr, fstr+1);
               i = strlen(fstr);
               setcolor(wstatus, MAINMENUCOLOR);
               mvwaddstr(wstatus, 0, 20, (i!=0) ? "*" : " ");
               setcolor(wstatus, FSTRCOLOR);
               mvwaddstr(wstatus, 0, 21, fstr);
               setcolor(wstatus, STATUSCOLOR);
               wclrtoeol(wstatus);
               wrefresh(wstatus);
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
            fchr = fstr[0];
            fstr[0] = '\0';
            wmove(wstatus, 0, 20);
            wclrtoeol(wstatus);
            wrefresh(wstatus);
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
            if (MOUSE_WHEEL_UP)
            {
                c = KEY_UP;
                unget = TRUE;
            }
            else if (MOUSE_WHEEL_DOWN)
            {
                c = KEY_DOWN;
                unget = TRUE;
            }
            break;
#endif
        case CTRL_F:
            getfstr();
            search(curr, RXFORW);
            if ((curr-ctop) >= r)
               ctop = curr - (r-1);
            break;
        case CTRL_D:
            getdupstr();
            if (field == 0)
               search(curr, RXFORW);
            else
               searchfield(curr, field);
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
            copy();
            break;
        case CTRL_V:
            paste();
            break;
        case CTRL_X:
            if (wr == TRUE)
            {
               ro = (ro == TRUE) ? FALSE : TRUE;
               titlemsg(datfname);
            }
            break;
        case CTRL_Z:
            safe = (safe == TRUE) ? FALSE : TRUE;
            titlemsg(datfname);
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
               curr = i;
               clsbody();
               wrefresh(wbody);
            }
            break;
        case CTL_LEFT:
            if ((field > 0) && !ro)
               reorder(curr, TRUE);
            break;
        case CTL_RIGHT:
            if ((field < cols) && !ro) 
               reorder(curr, FALSE);
            break;
        case ALT_LEFT:
            if (field > 0)
               reordall(TRUE);
            break;
        case ALT_RIGHT:
            if (field < cols)
               reordall(FALSE);
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
        case CTRL_G:
            gorec();
            if ((curr < ctop) || (curr >= (ctop+r)))
                	ctop = topset(curr, r);
            break;
        case KEY_SLEFT:
            if (!ro)
                align(field, curr, 1);
            break;
        case KEY_SRIGHT:
            if (!ro)
                align(field, curr, 3);
            break;
        case KEY_SHOME:
            if (!ro)
            {
                align(field, curr, 1);
                align(field, curr, 2);
            }
            break;
        case ALT_UP:
            xchange(FALSE);
            break;
        case ALT_DOWN:
            xchange(TRUE);
            break;
        case CTL_UP:
            if (ctop > 0)
                ctop--;
            break;
        case CTL_DOWN:
            if (ctop < (reccnt-r))
                ctop++;
            break;
        case ALT_I:
            insfield();
            break;
        case ALT_D:
            delfield();
            break;
        case CTRL_S:
            change();
            break;
        case ALT_S:
            schange();
            break;
        case ALT_C:
            calc(FALSE);
            break;
        case ALT_X:
            calc(TRUE);
            break;
        case ALT_Y:
            evalall();
            break;
        case CTRL_O:
            count(FALSE);
            break;
        case ALT_O:
            count(TRUE);
            break;
        case CTL_END:
            gominmax(FALSE);
            ctop = topset(curr, r);
            break;
        case CTL_HOME:
            gominmax(TRUE);
            ctop = topset(curr, r);
            break;
        case ALT_END:
            golen(FALSE);
            ctop = topset(curr, r);
            break;
        case ALT_HOME:
            golen(TRUE);
            ctop = topset(curr, r);
            break;
        case ALT_P:
            changecolor();
            break;
        case CTRL_Q:
            findequal(FALSE);
            ctop = topset(curr, r);
            break;
        case ALT_Q:
            findequal(TRUE);
            ctop = topset(curr, r);
            break;
//#if !defined(__unix) || defined(__DJGPP__)
        case CTRL_W:
            lat2(rows[curr], 1);
            flagmsg();
            break;
        case ALT_W:
            lat2(rows[curr], 0);
            flagmsg();
            break;
//#endif
        case CTRL_R:
            curr = popundo(curr);
            if ((curr < ctop) || (curr >= (ctop+r)))
                	ctop = topset(curr, r);
            break;
        case ALT_R:
            curr = redo(curr);
            if ((curr < ctop) || (curr >= (ctop+r)))
                	ctop = topset(curr, r);
            break;
        default:
            if ((c == 0x81) || (c == 0xEB) || (c == 0x1FB))
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
    werase(wstatus);
}

void DoOpen(void)
{
    if (!ro)
    {
        if (modified == TRUE)
            if (savefile(datfname, 0) != 0)
               return;
    }
    strcpy(fnam, datfname);
    if (getfname(TRUE, "File to open:", fnam, 50))
    {
        if (loadfile(fnam) == 0)
            titlemsg(fnam);
    }
}

void DoSave(void)
{
    if (ro && !cry)
        return;
    strcpy(fnam, datfname);
    if (getfname(TRUE, "Save to file:", fnam, 50))
    {
        if (savefile(fnam, 0) == 0)
            titlemsg(fnam);
    }
}

void newdb(void)
{
    create(FNAME);
}

void DoAppend(void)
{
    if (ro || cry)
        	return;

    strcpy(fnam, "?");
    if (getfname(TRUE, "Get from file:", fnam, 50))
    {
        if (getfile(fnam) == 0)
        {
            putmsg("File: ", fnam, " loaded.");
            flagmsg();
        }
    }
}


void txted(void)
{
    BUFDEF;

    if (!ro && !safe)
    {
        if ((yesno("Directly editing! Are You sure? (Y/N):")) == 0)
            return;
        if (modified == TRUE)
            if (savefile(datfname, 0) != 0)
               return;
        def_prog_mode();
        endwin();
        strcpy(buf, progname);
        strcat(buf, " -e ");
        strcat(buf, datfname);
        system(buf);
        (int)loadfile(datfname);
        reset_prog_mode();
        colorbox(wtitl, TITLECOLOR, 0);
        titlemsg(datfname);
        redraw();
        flagmsg();
    }
}

void tsv_select(void)
{
    int i, j;

    if (cry)
        	return;

    getfstr();
    if (strlen(fstr))
    {
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
        filtered = TRUE;
    }
    if (filtered)
    {
        tmpdat.total = reccnt;
        for (i=0, j=0; i<=reccnt; i++)
        {
            tmpdat.ptr[i] = rows[i];
            tmpdat.idx[i] = -1;
            tmpdat.flag[i] = flags[i];
            if (flags[i])
            {
                rows[j] = rows[i];
                tmpdat.idx[j] = i;
                flags[j] = 1;
                j++;
            }
        }
        rows[j] = rows[reccnt];
        reccnt = j;
        curr = 0;
        edit();
        reccnt = tmpdat.total;
        for (i=0; i<j; i++)
            	tmpdat.ptr[tmpdat.idx[i]] = rows[i];
        for (i=0; i<=reccnt; i++)
        {
            rows[i] = tmpdat.ptr[i];
            flags[i] = tmpdat.flag[i];
        }
        curr = 0;
    }
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
void txthelp(void), limits(void);

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
    { "Import", DoAppend, "Append data" },
    { "eXport", selected, "Restricted set (restart)" },
    { "Text", txted, "Edit as text" },
    { "Exit", bye, "Quit" },
    { "", (FUNC)0, "" }
};

menu SubMenu1[] =
{
    { "Edit", edit, "File edit" },
    { "View", brows, "Browse file" }, 
    { "Go", gorec, "Go to record" },
    { "Change", change, "Replace string" },
    { "scHange", schange, "Selectively change" },
    { "seLect", tsv_select, "Collated rows (with previous set)" },
    { "filteR", tsv_filter, "Choose records (restart with new file)" },
    { "Delimit", unlimit, "Remove delimiters" },
    { "Terminate", delimit, "Add delimiters" },
    { "seParate", dosep, "Set field separator" },
    { "adJust", doindent, "Align left/right/center" },
    { "Sort", dosort, "Sort the whole file" },
    { "Field", dosortby, "Sort by other field" },
    { "nUm", dosortnum, "Sort natural order" },
    { "Modify", modstru, "Restructuring" },
    { "crYpt", docrypt, "Code/decode" },
    { "Calc", calcall, "Evaluate the whole column" },
    { "tOtal", dosum, "Aggregate" },
//#if !defined(__unix) || defined(__DJGPP__)
    { "code", docode, "Switch coding in the entire file" },
//#endif
    { "", (FUNC)0, "" }
};

menu SubMenu2[] =
{
    { "Keys", subfunc1, "General keys" },
    { "input keys", edithelp, "Keys in edit mode" },
    { "Edit keys", txthelp, "Keys in texteditor" },
    { "Regexp", reghelp, "Regular expression help" },
    { "Options", opthelp, "Command line options" },
    { "Limits", limits, "Maximums & conditions" },
    { "Colors", changecolor, "Change color set" },
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
        "  Ctrl-Ins:  insert line (C-+)  \t Ctrl/Alt-A:  mark/filter all",
        "   Alt-Ins:  duplicate   (A-+)  \t   Ctrl-C/V:  copy/paste",
        "  Ctrl-Del:  delete line        \t      Alt-P:  change colours",
        " (C-)Enter:  edit field/s       \t Ctrl/Alt-E:  modify field/s",
        "    Letter:  search (? mask)    \t Ctrl/Alt-U:  uppercase/init",
        " ?+letters:  look only alnum    \t Ctrl/Alt-L:  lower/initial",
        "  Ctrl-F/D:  regexp search      \t C/A-arrows:  reorder fields",
        "     Alt-F:  seek curr field    \t Shft-arrow:  align left/right",
        " Tab/C-Tab:  find next          \t  Shft-Home:  adjust center",
        "  Shft-Tab:  previous           \t  Alt-Up/Dn:  move back/forward",
        "Bksp/S-Del:  del fstr back/first\t Ctrl/Alt-Q:  search equivalent",
        "  Del/Home:  clear fstr         \t    Alt-I/D:  ins/remove field",
        "    Ctrl-G:  goto line          \t Ctrl/Alt-O:  count subs/field",
        "Ctrl/Alt-S:  replace/change     \t   C/A-Home:  go max/longest",
//#if !defined(__unix) || defined(__DJGPP__)
        " Alt-C/X/Y:  calculate/fld/cols \t    C/A-End:  go min/shortest",
        "Ctrl/Alt-W:  to/from Latin-2    \t Ctrl-Up/Dn:  shift screen",
//#else
//        "     Alt-C:  calculate          \t    C/A-End:  go min/shortest",
//        "   Alt-X/Y:  calc fld/cols      \t Ctrl-Up/Dn:  shift screen",
//#endif
        "Ctrl/Alt-R:  undo/redo (max 10) \t  A-T/A-Del:  undo line (max 1)"
    };
    int i;
    int j=17;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/3, (bodywidth()-72)/2, j+2, 72);
#ifndef __MINGW_VERSION
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
#endif
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 1, s[i]);
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
        "   Home/End:\tgo to 1'st char/EOL",
        "    Up/Down:\tprevious/next field",
        "  PgUp/PgDn:\tsave & prev/next rec",
        "  Ctl-Up/Dn:\tscroll background",
        "   Del/Bksp:\tdelete char/backward",
        "   Ctrl-End:\tdelete from cursor",
        "     Ctrl-W:\tdelete word back",
        " Ctl-arrows:\tskip word",
        "   Ctrl-C/V:\tcopy/paste",
        "     Ctrl-B:\tpaste fstr",
        "Ctl/Alt-U/L:\tupper-/lowercase",
#ifdef __MINGW_VERSION
        " Shft-Up/Dn:\tchange chars",
#endif
        "     Ctrl-D:\tformat date",
        "      Alt-C:\tcalculate",
        "      Alt-D:\tchange dot & colon",
        "Ctl-O/P/R/T:\to^ / O^ / u^ / U^",
        "   Ctrl-X/Y:\taccent/punctuation",
        "C-PgUp/PgDn:\tinc/dec num,dat (S+/-)",
        "        Esc:\tundo/cancel",
        "      Alt-P:\tchange colorset",
        "      Enter:\tmodify record"
    };
    int i;
#ifdef __MINGW_VERSION
    int j=21;
#else
    int j=20;
#endif
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/4, (bodywidth()-40)/2, j+2, 40);
#ifndef __MINGW_VERSION
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
#endif
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 2, s[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wstatus);
    wrefresh(wstatus);
    touchwin(wbody);
    wrefresh(wbody);
}

void txthelp(void)
{
    WINDOW *wmsg;
    char *s[] =
    {
        " ^A:\tgo to line",
        " ^B:\tbegin of block",
        " ^C:\tcopy block",
        " ^D:\tdelete block",
        " ^E:\tend of block",
        " ^F:\tfind",
        " ^G:\tget from file",
        " ^J:\tpgup",
        " ^K:\tpgdn",
        " ^N:\tsearch again",
        " ^O:\tgo bottom of text",
        " ^P:\tput to file",
        " ^Q:\tquote char",
        " ^R:\treplace string",
        " ^S:\tsave file",
        " ^T:\tgo top of text",
        " ^V:\tmove block",
        " ^X:\texit",
        " ^Y:\tdelete line"
    };
    int i;
    int j=19;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/4, (bodywidth()-28)/2, j+2, 28);
#ifndef __MINGW_VERSION
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
#endif
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 2, s[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wstatus);
    wrefresh(wstatus);
    touchwin(wbody);
    wrefresh(wbody);
}

void reghelp(void)
{
    WINDOW *wmsg;
    char *s[] =
    {
        "(?i)    At the beginning of the regex, match case-insensitive",
        "^       Match beginning of a buffer",
        "$       Match end of a buffer (or '\\x0a' when CRLF)",
        "()      Grouping and substring capturing",
        "\\s      Match whitespace",
        "\\S      Non-whitespace",
        "\\d      Decimal digit",
        "\\n      New line character        \t \\b      Backspace char",
        "\\r      Line feed                 \t \\f      Form feed",
        "\\t      Horizontal tab            \t \\v      Vertical tab",
        "+       One or more times (greedy) \t +?   1|>1 (non-greedy)",
        "*       Zero or more times (greedy)\t *?   0|>0 (non-greedy)",
        "?       Zero or once (non-greedy)",
        "x|y     x or y (alternation operator)",
        "\\meta   One of the meta character: ^$().[]*+?|\\",
        "\\xHH    Byte with hex value 0xHH, e.g. \\x4a",
        "[...]   Any character from set. Ranges like [a-z] supported",
        "[^...]  Any character but ones from set"
    };
    int i;
    int j=18;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/4, (bodywidth()-65)/2, j+2, 65);
#ifndef __MINGW_VERSION
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
#endif
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 2, s[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

void limits(void)
{
    WINDOW *wmsg;
    char buf[40];
    char *s[] =
    {
        "  Max length of rows: ",
        "   Number of records: ",
        "    Number of fields: ",
        "Length of field name: ",
        "     Field separator: ",
        "           Read only: ",
        "              Safety: ",
        "              Locked: ",
        "             Crypted: "
    };
    int n[] =
    {
        MAXSTRLEN,
        MAXROWS,
        MAXCOLS,
        MAXFLEN,
        csep,
        ro,
        safe,
        locked,
        cry
    };
    int i;
    int j=9;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/3, (bodywidth()-34)/2, j+2, 34);
#ifndef __MINGW_VERSION
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
#endif
    for (i=0; i<j; i++)
    {
        strcpy(buf, s[i]);
        if (i == 4)
        {
            switch (csep)
            {
                case '\t':
                    strcat(buf, "TAB");
                    break;
                case ';':
                    strcat(buf, "SEMI");
                case ',':
                    strcat(buf, "COLON");
            }
        }
        else
            itoa(n[i], buf+strlen(buf), 10);
        mvwaddstr(wmsg, i+1, 2, buf);
    }
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

static char *hlpstrs[] =
{
    "-r        Read-only mode",
    "-x        Mixed mode",
    "-y        Locked mode",
    "-z        Safety on",
    "-t        Top",
    "-b        Bottom",
    "-n <num>  Go to num'th row",
    "-s <str>  Search str",
    "          or \"-s (regexp)\"",
    "          or \"-S (regexp)\" to extract",
    "-l <str>  List str found",
    "          or \"-L\" with header",
    "-d <,|;>  Set separator to ',' or ';'",
    "-e        Edit as text",
    "-h        Help",
    "-v        Version",
    "",
    NULL
};

void help(void)
{
    int i;

    for (i=0; hlpstrs[i]; i++)
        printf("\t%s\n", hlpstrs[i]);
}

void opthelp(void)
{
    WINDOW *wmsg;
    int i;
    int j=16;
    
    wmsg = mvwinputbox(wbody, (bodylen()-j)/3, (bodywidth()-40)/2, j+2, 40);
#ifndef __MINGW_VERSION
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
#endif
    for (i=0; i<j; i++)
        mvwaddstr(wmsg, i+1, 2, hlpstrs[i]);
    wrefresh(wmsg);
    (void)toupper(waitforkey());
    delwin(wmsg);
    touchwin(wbody);
    wrefresh(wbody);
}

void subfunc2(void)
{
    WINDOW *wmsg;
    int c = 0;
    int begx, begy;
    BUFDEF = "";
    char *brows;
    char *s[] =
    {
        "TSVdb v."VERSION,
        "Simple tab separated text database",
        " Based on PDCurses TUIdemo sample",
        " With SLRE lib http://cesanta.com",
        "       "URL
    };
    
    wmsg = mvwinputbox(wbody, (bodylen()-5)/3, (bodywidth()-40)/2, 7, 40);
    wborder(wmsg, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwaddstr(wmsg, 1,13, s[0]);
    mvwaddstr(wmsg, 2, 3, s[1]);
    mvwaddstr(wmsg, 3, 3, s[2]);
    mvwaddstr(wmsg, 4, 3, s[3]);
    mvwaddstr(wmsg, 5, 3, s[4]);
    wrefresh(wmsg);
#ifdef __MINGW_VERSION
    pause(1);
#else
    sleep(1);
#endif
    c = waitforkey();
#ifdef PDCURSES
    if (c == KEY_MOUSE)
    {
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
            getbegyx(wmsg, begy, begx);
            if ((MOUSE_Y_POS == begy+5)
            &&  (MOUSE_X_POS > begx+8) && (MOUSE_X_POS < begx+30))
            {
#ifdef __MINGW_VERSION
                brows = "explorer.exe";
#else
                brows = getenv("browser");
                if (brows == NULL)
                    brows = getenv("BROWSER");
#endif
                if (brows != NULL)
                {
                    strcpy(buf, brows);
                    strcat(buf, " ");
                    strcat(buf, URL);
                    #ifdef __MINGW_VERSION 	//Glib err! :(
                    system(buf);
                    #endif
                }
            }
        }
    }
#endif
    delwin(wmsg);
    touchwin(wbody);
#ifdef PDCURSES
    mouse_set(ALL_MOUSE_EVENTS);
    PDC_save_key_modifiers(TRUE);
    PDC_return_key_modifiers(TRUE);
#endif
    wrefresh(wbody);
}

void changecolor(void)
{
    int i, j;
    menu *mp = MainMenu;

    if (!has_colors())
        	return;

    if (colorset)
        	initcolor();
    else
        	initcolo2();
    colorset = !colorset;
    colorbox(wtitl, TITLECOLOR, 0);
    colorbox(wmain, MAINMENUCOLOR, 0);
    colorbox(wbody, BODYCOLOR, 0);
    colorbox(wstatus, STATUSCOLOR, 0);
    for (i=0; i<bh; i++)
    {
        for (j=0; j<bw; j++)
             	mvwaddstr(wbody, i, j, "x");
    }
    wmove(wbody, bh, 1);
    for (j=0; j<(bw-1); j++)
         	waddch(wbody, 'x');
    touchwin(wbody);
    wrefresh(wbody);
    idle();
    titlemsg(datfname);
    flagmsg();
    menudim(mp, &i, &j);
    repaintmainmenu(j, mp);
    redraw();
}


/*** start main ***/

int main(int argc, char **argv)
{
    int i = 0;
    int c;
    char s[MAXSTRLEN] = "";

    strncpy(progname, argv[0], MAXSTRLEN-1);
    curr = 0;
    opterr = 0;
    while ((c=getopt(argc, argv, "HhRrVvXxYyZzTtBbEeN:n:D:d:S:s:L:l:?")) != -1)
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
        case 'z':
        case 'Z':
          safe = TRUE;
          break;
        case 'v':
        case 'V':
          printf("\nTSVdb v.%s %s by begyu (%s)\n", VERSION, __DATE__, URL);
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
          if (fstr[0] == '(')
          {
              regex = TRUE;
              unkeys[3] = CTRL_F;
              i = strlen(fstr);
              if (fstr[i-1] != ')')
              {
                  strcat(fstr, ")");
              }
              if (c == 'S')
              {
                  strncpy(regstr, fstr, MAXFLEN);
                  unkeys[2] = 'l';
                  unkeys[3] = '\0';
              }
          }
          else
          {
              casestr(fstr, TRUE, TRUE);
              unkeys[4] = '\n';
              unkeys[5] = '\0';
          }
          pfind = TRUE;
          break;
        case 'l':
          i = -2;
        case 'L':
          strcpy(fstr, optarg);
          if (i != -2)
              i = -3;
          break;
        case '?':
          if ((toupper(optopt) == 'N')
          || (toupper(optopt) == 'S')
          || (toupper(optopt) == 'L')
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
          if (toupper(c) == 'N')
          {
              if (isdigit(optarg[0]))
              {
                  curr = atoi(optarg);
                  if (curr > 0)
                      curr--;
                  break;
              }
              else
                  fprintf (stderr, "Expected record number.\n");
          }
        case 'd':
        case 'D':
          if (toupper(c) == 'D')
          {
              if ((strcmp(optarg, COLSSEP) == 0)
              ||  (strcmp(optarg, SCLSSEP) == 0))
              {
                  csep = optarg[0];
                  ssep[0] = optarg[0];
                  break;
              }
              else
                  fprintf (stderr, "Expected ',' or ';' char.\n");
          }
        case 'h':
        case 'H':
          printf("\nUsage: %s [-r|x|y|z|t|b|e|h|v] [-n{row}] [-[s|l]{str}] "
            "[-d{sep}] [file]\n",
            (char *)basename(progname));
          if (toupper(c) == 'H')
              help();
          exit(c==':' ? -1 : 0);
        case 'e':
        case 'E':
          i = -1;
          break;
        default:
          break;
      }
    }
    if (optind < argc)
        strcpy(s, argv[optind++]);
    else
        strcpy(s, FNAME);
    setlocale(LC_ALL, "C");
    if (i == -1)
    {
       ed(s);
       return 0;
    }
    else if (i < -1)
    {
       copyfile(s, fstr, (i==-3));
       return 0;
    }
    init();
    if (loadfile(s) == 0)
       strcpy(datfname, s);
    else
    {
       if (create(s) != 0)
       {
          curs_set(1);
          endwin();
          return -1;
       }
    }
#ifdef __MINGW_VERSION
    SetConsoleTitle("TSVdb v."VERSION);
#endif
    signal(SIGINT, siginthandler);
    startmenu(MainMenu, datfname);
#ifdef __MINGW_VERSION
    EnableConsoleCloseButton();
#endif
    return 0;
}

#ifdef __cplusplus
}
#endif

