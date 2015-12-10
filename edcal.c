/***ED***/
/* frox25.no-ip.org/~mtve */
//#include <curses.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

/*#define KEY_BACKSPACE 0x008*/
#define TABSIZE 8
#define CHUNKSIZE 1024
#define STRMAX 64
#define BUFSIZE 256

#ifdef CTRL
#undef CTRL
#endif
#define CNTRL(a) ((a) & 31)

char    *text = 0;
int     text_size = 0;

char    file_name[STRMAX] = "", block_name[STRMAX] = "";
char    find_str[STRMAX] = "", replace_str[STRMAX] = "";

#define HLPSTR "A:go, B:begin, C:copy, D:del, E:end, F:find, G:get, "\
  "J:pgup, K:pgdn, N:again, \nO:bottom, P:put, Q:quote, R:repl, S:save, "\
  "T:top, V:move, X:exit, Y:delline"
char    help_str[2*STRMAX] = "";
#define KEY_ESC  0x1b

int     bos_pos = 0, eos_pos = 0, cur_pos = 0, eof_pos = 0, bow_line = 0;
int     bow_line_prev = 0, win_shift = 0, cur_line = 0, cur_y = 0, cur_x = 0;
int     is_changed = 0, ins_mode = 1, find_mode = 0;

#define ALIGN(x,mod)    (((x) / (mod) + 1) * (mod))
#define nexttab(x)      ALIGN (x, TABSIZE)
#define align_chunk(x)  ALIGN (x, CHUNKSIZE)

void    adduch(unsigned char ch)
{
//      if ((ch >= 32 && ch < 128) || ch >= 160)        /* ^C or ~C */
        if (ch >= 32)
                addch(ch);
        else {
                attron(A_BLINK);
//              addch(ch > 128 && ch < 159 ? ch - 32 : ch + 64);
                addch(ch + 64);
                attroff(A_BLINK);
        }
}

int     confirm(char *s)
{
        int     ch;

        move(LINES - 1, 0);
        attron(A_BOLD);
        addstr(s);
        attroff(A_BOLD);
        clrtoeol();
        refresh();
        ch = getch();
        return ch == 'y' || ch == 'Y';
}

void    hmsg(char *s)
{
        move(LINES - 1, 0);
        attron(A_BOLD);
        addstr(s);
        attroff(A_BOLD);
        clrtoeol();
        refresh();
        (void)getch();
}

int     enter_string(char *s, char *buf)
{
        int     b_len, ch, flag = 1;

        for (;;) {
                move(LINES - 1, 0);
                attron(A_BOLD);
                addstr(s);
                attroff(A_BOLD);
                for (b_len = 0; buf[b_len]; b_len++)
                        adduch(buf[b_len]);
                clrtoeol();
                refresh();
                ch = getch();
                switch (ch) {
                case CNTRL('Y'):
                        *buf = 0;
                        break;
                case CNTRL('Q'):
                        ch = getch();
                        goto ins_char;
                case KEY_BACKSPACE:
                case 8:
                        if (b_len)
                                buf[b_len - 1] = 0;
                        break;
                case '\r':
                        return 1;
                case CNTRL('X'):
                        return 0;
                default:
                        if (!iscntrl(ch)) {
                ins_char:       if (flag)
                                        *buf = b_len = 0;
                                if (b_len < STRMAX - 1) {
                                        buf[b_len] = ch;
                                        buf[b_len + 1] = 0;
                                }
                        } else
                                beep();
                        break;
                }
                flag = 0;
        }
        /* NOTREACHED */
}

int     error(char *s, ...)
{
        va_list args;
        char    buf[BUFSIZE];
        int     i = 0;

        va_start(args, s);
        i += snprintf(buf + i, BUFSIZE - i, "Error ");
        if (*s != '$')
                i += vsnprintf(buf + i, BUFSIZE - i, s, args);
        else {
                i += vsnprintf(buf + i, BUFSIZE - i, s + 1, args);
                i += snprintf(buf + i, BUFSIZE - i, ", %s", strerror(errno));
        }
        va_end(args);
        beep();
        confirm(buf);

        return 0;       /* convinient */
}

int     bol(int pos)
{
        while (pos && text[pos - 1] != '\n')
                pos--;
        return pos;
}

int     prevline(int pos)
{
        pos = bol(pos);
        return pos ? bol(pos - 1) : 0;
}

int     eol(int pos)
{
        while (pos < eof_pos && text[pos] != '\n')
                pos++;
        return pos;
}

int     nextline(int pos)
{
        pos = eol(pos);
        return pos < eof_pos ? pos + 1 : pos;
}

int     win_x(int line, int xx)
{
        int     i, x = 0;

        for (i = line; i < eof_pos && i < line + xx; i++)
                if (text[i] == '\n')
                        break;
                else if (text[i] == '\t')
                        x = nexttab(x);
                else
                        x++;
        return x;
}

int     pos_x(int line, int xx)
{
        int     i, x = 0;

        for (i = line; i < eof_pos && x < xx; i++)
                if (text[i] == '\n')
                        break;
                else if (text[i] == '\t')
                        x = nexttab(x);
                else
                        x++;
        return i;
}

void    show(void)
{
        int     i, m, t, j;

        /*
         * speed up scrolling
         */
        if (bow_line > bow_line_prev) {
                m = bow_line_prev;
                for (i = 0; m != bow_line && i < LINES; i++)
                        m = nextline(m);
                if (i < LINES)
                        scrl(i);
        } else if (bow_line < bow_line_prev) {
                m = bow_line_prev;
                for (i = 0; m != bow_line && i < LINES; i++)
                        m = prevline(m);
                if (i < LINES)
                        scrl(-i);
        }
        bow_line_prev = bow_line;

        erase();
        if (!text)
                return;
        for (m = bow_line, i = 0; m < eof_pos && i < LINES; i++) {
                m = pos_x(m, win_shift);
                move(i, 0);
#define EOS_COLS (i < LINES - 1 ? COLS : COLS - TABSIZE)
                for (j = 0; m < eof_pos && j < EOS_COLS; m++) {
                        if (m >= bos_pos && m < eos_pos)
                                attron(A_REVERSE);
                        else
                                attroff(A_REVERSE);
                        if (text[m] == '\n')
                                break;
                        else if (text[m] == '\t')
                                for (t = nexttab(j); j < t; j++)
                                        addch(' ');
                        else {
                                adduch(text[m]);
                                j++;
                        }
                }
                if (m >= bos_pos && m < eos_pos)
                        while (j++ < EOS_COLS)
                                addch(' ');
#undef EOS_COLS
                m = nextline(m);
        }
        attroff(A_REVERSE);
}

void    k_up(void)
{
        cur_line = prevline(cur_line);
        cur_pos = pos_x(cur_line, cur_x + win_shift);
}

void    k_down(void)
{
        if (eol(cur_pos) < eof_pos) {
                cur_line = nextline(cur_line);
                cur_pos = pos_x(cur_line, cur_x + win_shift);
        }
}

int     ins_mem(int size)
{
        char    *p;
        int     i;

        if (!text || eof_pos + size > text_size) {
                i = align_chunk(eof_pos + size);
                p = realloc(text, i);
                if (!p)
                        return error("- no memory");
                text = p;
                text_size = i;
        }
        /*
         * read last sentence in BUGS section of memcpy(3) in FreeBSD,
         * also bcopy(3) is not in ``ISO C''
         */
        for (i = eof_pos - 1; i >= cur_pos; i--)
                text[i + size] = text[i];
        eof_pos += size;
        if (bos_pos >= cur_pos)
                bos_pos += size;
        if (eos_pos > cur_pos)
                eos_pos += size;
        is_changed = 1;
        return 1;
}

void    del_mem(int pos, int size)
{
        int     i;
        char    *p;

        for (i = pos + size; i < eof_pos; i++)  /* read comment to ins_mem() */
                text[i - size] = text[i];
        eof_pos -= size;
        is_changed = 1;
#define del_pos(p) (p > pos + size ? p -= size : p > pos ? p = pos : p)
        del_pos(bos_pos);
        del_pos(eos_pos);
        del_pos(cur_pos);
        del_pos(bow_line);
        del_pos(bow_line_prev);
#undef del_pos
        i = align_chunk(eof_pos);
        if (i < text_size) {
                p = realloc(text, i);
                if (!p) {
                        error("- realloc to decrease failed?");
                        return;
                }
                text = p;
                text_size = i;
        }
}

void    ins_ch(char ch)
{
        if (!ins_mode && cur_pos < eof_pos) {
                if (ch == '\n') {
                        cur_pos = nextline(cur_pos);
                        return;
                } else if (text[cur_pos] != '\n') {
                        is_changed = 1;
                        goto a;
                }
        }
        if (ins_mem(1))
a:              text[cur_pos++] = ch;
}

void    k_copyblock(void)
{
        if (eos_pos <= bos_pos || (cur_pos > bos_pos && cur_pos < eos_pos))
                beep();
        else if (ins_mem(eos_pos - bos_pos))
                strncpy(text + cur_pos, text + bos_pos, eos_pos - bos_pos);
}

void    k_moveblock(void)
{
        int     i;

        if (eos_pos <= bos_pos || (cur_pos > bos_pos && cur_pos < eos_pos)) {
                beep();
                return;
        }
        k_copyblock();
        i = eos_pos - bos_pos;
        del_mem(bos_pos, i);
        bos_pos = cur_pos;
        eos_pos = cur_pos + i;
}

void    k_deleteblock(void)
{
        if (eos_pos <= bos_pos)
                beep();
        else
                del_mem(bos_pos, eos_pos - bos_pos);
}

int     find_again(int flag)
{
        int     f_len, i;

        f_len = strlen(find_str);
        if (!f_len)
                return 0;
        for (i = cur_pos + flag; i <= eof_pos - f_len; i++)
                if (!strncmp(text + i, find_str, f_len))
                        break;
        if (i > eof_pos - f_len)
                beep();
        else
                cur_pos = i;
        return i <= eof_pos - f_len;
}

int     k_find(void)
{
        if (!enter_string("Search for: ", find_str) || !*find_str)
                return 0;
        find_mode = 1;
        return find_again(0);
}

void    replace_again(void)
{
        int     i;

        if (!find_again(0))
                return;
        del_mem(cur_pos, strlen(find_str));
        if (!*replace_str)
                return;
        i = strlen(replace_str);
        if (ins_mem(i)) {
                strncpy(text + cur_pos, replace_str, i);
                cur_pos += i;
        }
}

void    k_replace(void)
{
        if (!k_find())
                return;
        if (!enter_string("Replace to: ", replace_str))
                return;
        find_mode = 0;
        replace_again();
}

void    k_again(void)
{
        if (find_mode)
                find_again(1);
        else
                replace_again();
}

int     load(char *name)
{
        FILE    *f;
        int     i, j;

        f = fopen(name, "r");
        if (!f)
                return error("$load file \"%s\"", name);
        if (fseek(f, 0, SEEK_END))
        {
                fclose(f);
                return error("$seek");
        }
        i = ftell(f);
        if (ins_mem(i)) {
                if (fseek(f, 0, SEEK_SET))
                {
                        fclose(f);
                        return error("$seek");
                }
                if ((j=fread(text + cur_pos, 1, i, f)) < i)
                {
                        for ( ; j<i; j++)
                              text[j] = 0;
/*                        return error("$read");*/
                }
        } else
                i = 0;
        fclose(f);
        return i;
}

int     save(char *name, int pos, int size)
{
        FILE    *f;

        f = fopen(name, "w");
        if (!f)
                return error("$save file \"%s\"", name);
        if (fwrite(text + pos, 1, size, f) < size)
        {
                fclose(f);
                return error("$write");
        }
        if (fclose(f))
                return error("$close");
        return 1;
}

void    k_save(void)
{
        if (!enter_string("Enter file name to save: ", file_name))
                return;
        if (save(file_name, 0, eof_pos))
                is_changed = 0;
}

void    k_getblock(void)
{
        if (!enter_string("Enter file name to load block: ", block_name))
                return;
        eos_pos = load(block_name) + cur_pos;
        bos_pos = cur_pos;
}

void    k_putblock(void)
{
        if (bos_pos >= eos_pos)
                return;
        if (!enter_string("Enter file name to save block: ", block_name))
                return;
        save(block_name, bos_pos, eos_pos - bos_pos);
}

void    goto_line(int l)
{
        for (cur_pos = 0; --l > 0 && cur_pos < eof_pos;)
                cur_pos = nextline(cur_pos);
}

void    k_goto(void)
{
        char    buf[STRMAX];

        *buf = 0;
        if (!enter_string("Goto line: ", buf))
                return;
        if (*buf)
                goto_line(atoi(buf));
        else
                cur_pos = bos_pos;
}

/* ARGSUSED0 */
void    done(int sig)
{
        endwin();
        exit(0);
}

void    edinit(void)
{
        signal(SIGINT, done);
        initscr();
        keypad(stdscr, TRUE);
        scrollok(stdscr, TRUE);
        idlok(stdscr, TRUE);
        nonl();
        raw();
        noecho();
        refresh();
}

void    norm_cur(void)
{
        int     i;

        cur_line = bol(cur_pos);
        while (cur_line < bow_line)
                bow_line = prevline(bow_line);
        cur_y = 0;
        for (i = bow_line; i < cur_line; i = nextline(i))
                cur_y++;
        for (; cur_y >= LINES; cur_y--)
                bow_line = nextline(bow_line);
        cur_x = win_x(cur_line, cur_pos - cur_line) - win_shift;
        while (cur_x < 0) {
                cur_x += TABSIZE;
                win_shift -= TABSIZE;
        }
        while (cur_x >= COLS) {
                cur_x -= TABSIZE;
                win_shift += TABSIZE;
        }
}

int ed(char *f)
{
        int     i, ch;

        edinit();
        if (f != NULL) {
                strncpy(file_name, f, STRMAX - 1);
                file_name[STRMAX - 1] = 0;
                load(file_name);
                is_changed = 0;
        }
        for (;;) {
                show();
                move(cur_y, cur_x);
                refresh();
                ch = getch();
                switch (ch) {
                case 129: //0x81=''
                case 144: //0x90=''
                case 395: //0x8B='‹'
                case 394: //0x8A='Š'
                case 507: //0xFB='ű'
                case 491: //0xEB='ë'
                        ins_ch(ch);
                        break;
                case KEY_UP:
                        k_up();
                        break;
                case KEY_DOWN:
                        k_down();
                        break;
                case KEY_LEFT:
                        if (cur_pos)
                                cur_pos--;
                        break;
                case KEY_RIGHT:
                        if (cur_pos < eof_pos)
                                cur_pos++;
                        break;
                case KEY_PPAGE: case CNTRL('J'):
                        for (i = 1; i < LINES; i++)
                                k_up();
                        break;
                case KEY_NPAGE: case CNTRL('K'):
                        for (i = 1; i < LINES; i++)
                                k_down();
                        break;
                case CTL_HOME:
                        cur_line = 0;
                        cur_pos  = 0;
                        break;
                case KEY_DC:    /* del */
                        if (cur_pos < eof_pos)
                                del_mem(cur_pos, 1);
                        break;
                case KEY_BACKSPACE:
                case 8:
                        if (cur_pos)
                                del_mem(--cur_pos, 1);
                        break;
                case KEY_HOME:
                        cur_pos = cur_line;
                        break;
                case KEY_END:
                        cur_pos = eol(cur_pos);
                        break;
                case CNTRL('X'):
                        if (!is_changed || confirm("Discard changes and exit? (y/N):"))
                                done(0);
                        break;
                case CNTRL('T'):        /* go Top */
                        bow_line = cur_pos = 0;
                        break;
                case CTL_END:
                case CNTRL('O'):        /* go bOttom */
                        cur_pos = eof_pos;
                        break;
                case CNTRL('Y'):        /* del line */
                        del_mem(cur_line, nextline(cur_line) - cur_line);
                        break;
                case CNTRL('B'):        /* mark Begin of block */
                        bos_pos = cur_pos;
                        break;
                case CNTRL('E'):        /* mark End of block */
                        eos_pos = cur_pos;
                        break;
                case CNTRL('Q'):        /* Quote char */
                        ins_ch(getch());
                        break;
                case CNTRL('C'):
                        k_copyblock();
                        break;
                case CNTRL('D'):
                        k_deleteblock();
                        break;
                case CNTRL('V'):
                        k_moveblock();
                        break;
                case CNTRL('S'):
                        k_save();
                        break;
                case CNTRL('F'):
                        k_find();
                        break;
                case CNTRL('R'):
                        k_replace();
                        break;
                case CNTRL('N'):
                        k_again();
                        break;
                case CNTRL('P'):
                        k_putblock();
                        break;
                case CNTRL('G'):
                        k_getblock();
                        break;
                case KEY_IC:
                case CTL_PADPLUS:
                        ins_mode ^= 1;
                        curs_set(ins_mode ? 2 : 1);
                        break;
                case CNTRL('A'):
                        k_goto();
                        break;
                case KEY_ESC:
                case KEY_F(1):
                        hmsg(HLPSTR);
                        break;
                case '\r':
                        ch = '\n';
                        /* FALLTHRU */
                default:
                        if (!iscntrl(ch) || ch == '\t' || ch == '\n')
                                ins_ch(ch);
                        else
                                beep();
                        break;
                }
                norm_cur();
        }
        /* NOTREACHED */
        return 0;
}
/*END_ED*/


/****CALCU****/
bool calcerr = FALSE;

/*#include <stdio.h>*/
/*#include <stdlib.h>*/
/*#include <ctype.h>*/
#include <math.h>

size_t proccessSum(double *);

enum
{
	LT_NUMBER,
	LT_OPERATOR,
	LT_NONE
} LexType;

enum
{
	ANNERR_NONE,
	ANERR_SYNTAX,
	ANERR_DIVBYZERO,
	ANERR_BRACKETS,
	ANERR_NUM
} AnalError;

char token[256];
char *expr;
size_t i = 0;
size_t tokenType;

size_t getToken()
{
/*	size_t len = strlen(expr);*/
	size_t j = 0;
	size_t res = LT_NONE;

	if(expr[i] == '\0')
		return 0;

	while(isspace(expr[i]))
		++i;

	// number
	if(isdigit(expr[i]) || expr[i] == '.')
	{
		do
		{
			token[j] = expr[i];
			++i; ++j;
		}
		while(isdigit(expr[i]) || expr[i] == '.');
		token[j] = '\0';
		res = LT_NUMBER;
	}
	else //operator
	if(strchr("+-*/^()", expr[i]))
	{
		token[0] = expr[i];
		token[1] = '\0';
		res = LT_OPERATOR;
		++i;
	}
	else
	{
/*		printf("Error parsing first symbol\n");*/
		calcerr = TRUE;
	}

	tokenType = res;
	return res;
}

size_t proccessAtom(double *answ)
{
/*	char op;*/
/*	double t = 0;*/

	if(tokenType == LT_NUMBER)
	{
		*answ = atof(token);
		getToken();
	}
	else
		return ANERR_SYNTAX;
	return 0;
}


size_t proccessBrackets(double *answ)
{
	size_t r;
	char op = *token;

	if(op == '(')
	{
		getToken();
		if((r = proccessSum(answ)) != 0)
			return r;
		op = *token;
		if(op != ')')
			return ANERR_BRACKETS;
		getToken();
	}
	else 
	if((r = proccessAtom(answ)) != 0)
		return r;
	
	return 0;
}

size_t proccessUnary(double *answ)
{
	char op;
	size_t r;
/*	double t = 0;*/
	
	op = *token;
	if(op == '+' || op == '-')
	{
		getToken();
	}
	if((r = proccessBrackets(answ)) != 0)
		return r;

	if(op == '-')
		(*answ) = -(*answ);
	
	return 0;
}

size_t proccessPow(double *answ)
{
	char op;
	size_t r;
	double t = 0;
	
	if((r = proccessUnary(answ)) != 0)
		return r;

	op = *token;
	while(op == '^')
	{
		getToken();
		if((r = proccessUnary(&t)) != 0)
			return r;
		*answ = pow(*answ, t);
		op = *token;
	}

	return 0;
}

size_t proccessMul(double *answ)
{
	char op;
	size_t r;
	double t = 0;
	
	if((r = proccessPow(answ)) != 0)
		return r;

	op = *token;
	while(op == '*' || op == '/')
	{
		getToken();
		if((r = proccessPow(&t)) != 0)
			return r;
		switch(op)
		{
			case '*':
				*answ *= t;
			break;

			case '/':
				if(t == 0)
					return ANERR_DIVBYZERO;
				*answ /= t;
			break;
		}
		op = *token;
	}

	return 0;
}

size_t proccessSum(double *answ)
{
	char op;
	double t = 0;
	size_t r;
	if((r = proccessMul(answ)) != 0)
		return r;

	op = *token;
	while(op == '+' || op == '-')
	{
		getToken();
		if((r = proccessMul(&t)) != 0)
			return r;
		switch(op)
		{
			case '+':
				*answ += t;
			break;

			case '-':
				*answ -= t;
			break;
		}
		
		op = *token;
	}

	return 0 ;
}

double calcExpression(char *str)
{
	double res = 0;
	size_t r = 0;
	expr = str;
	if((expr[0] == 0) || (expr[0] == 32 && expr[1] == 0))
	{
		strcpy(str, "0.000");
		return res;
	}
	i = 0;
	calcerr = FALSE;
	getToken();
	if(calcerr)
		return res;
	if((r = proccessSum(&res)) != 0)
	{
		char buf[64];
		switch(r)
		{
			case ANERR_SYNTAX:
				strcpy(buf, "Syntax error");
			break;
			case ANERR_DIVBYZERO:
				strcpy(buf, "Division by zero");
			break;
			case ANERR_BRACKETS:
				strcpy(buf, "Brackets pairing error");
			break;
		}
/*		printf("Calculation error %d (%s)\n", r, buf);*/
		sprintf(str, "Calculation error %d (%s)", (int)r, buf);
	}else
/*		printf("%s =  %lf\n", expr, res);*/
		sprintf(str, "%s = %lf %c", expr, res, '\0');
	return res;
}

void calcexp(char *str)
{
	double res = 0;
	size_t r = 0;
	expr = str;
	i = 0;
	calcerr = FALSE;
	getToken();
	if(calcerr)
		return;
	if((r = proccessSum(&res)) == 0)
		sprintf(str, "%lf %c", res, '\0');
}
/*END_CALCU*/
