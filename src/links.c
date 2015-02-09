/*****************************************************\
  LINKS.C
  By: Jesse McClure (c) 2015
  See slider.c or COPYING for license information
\*****************************************************/

#include "slider.h"

static PopplerDocument *_pdf;
static PopplerRectangle *_pdf_to_x(PopplerRectangle *, double, double);
static void _goto(PopplerAction *);
static void _launch(PopplerAction *);
static void _movie(PopplerAction *);
static void _none(PopplerAction *);
static void _uri(PopplerAction *);
static void _named(PopplerAction *);

static void (*handler[POPPLER_ACTION_JAVASCRIPT + 1]) (PopplerAction *) = {
	[POPPLER_ACTION_UNKNOWN]       = _none,
	[POPPLER_ACTION_NONE]          = _none,
	[POPPLER_ACTION_GOTO_DEST]     = _goto,
	[POPPLER_ACTION_GOTO_REMOTE]   = _none,
	[POPPLER_ACTION_LAUNCH]        = _launch,
	[POPPLER_ACTION_URI]           = _uri,
	[POPPLER_ACTION_NAMED]         = _named,
	[POPPLER_ACTION_MOVIE]         = _movie,
	[POPPLER_ACTION_RENDITION]     = _none, // sound
	[POPPLER_ACTION_OCG_STATE]     = _none,
	[POPPLER_ACTION_JAVASCRIPT]    = _none,
};

int link_follow(PopplerActionType type) {
	_pdf = render_get_pdf_ptr();
	PopplerPage *page = poppler_document_get_page(_pdf, cur);
	PopplerAction *act = NULL;
	PopplerRectangle rect;
	PopplerLinkMapping *lmap;
	GList *links, *list;
	links = poppler_page_get_link_mapping(page);
	if (type) {
		for (list = links; list; list = list->next) {
			lmap = list->data;
			act = lmap->action;
			if (lmap->action->type == type)
				handler[lmap->action->type](lmap->action);
		}
		poppler_page_free_link_mapping(links);
		return 0;
	}
	XEvent ev;
	double pdfw, pdfh;
	poppler_page_get_size(page, &pdfw, &pdfh);
	PopplerRectangle *r = NULL;
	XDefineCursor(dpy, presWin, XCreateFontCursor(dpy, 68));
	XGrabPointer(dpy, root, true, PointerMotionMask | ButtonPressMask,
			GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	while (!XNextEvent(dpy, &ev)) {
		if (ev.type == ButtonPress) break;
		else if (ev.type == MotionNotify && !act) {
			for (list = links; list; list = list->next) {
				lmap = list->data;
				act = lmap->action;
				r = _pdf_to_x(&lmap->area, pdfw, pdfh);
				if (	(ev.xmotion.x < r->x1) || (ev.xmotion.x > r->x2) ||
						(ev.xmotion.y < r->y1) || (ev.xmotion.y > r->y2) )
					act = NULL;
				else break;
			}
			if (act)
				XDefineCursor(dpy, presWin, XCreateFontCursor(dpy, 60));
		}
		else if (ev.type == MotionNotify) {
			if (	(ev.xmotion.x < r->x1) || (ev.xmotion.x > r->x2) ||
					(ev.xmotion.y < r->y1) || (ev.xmotion.y > r->y2) ) {
				act = NULL;
				XDefineCursor(dpy, presWin, XCreateFontCursor(dpy, 68));
			}
		}
	}
	XDefineCursor(dpy, presWin, None);
	XUngrabPointer(dpy, CurrentTime);
	if (act) handler[lmap->action->type](lmap->action);
	poppler_page_free_link_mapping(links);
	g_object_unref(page);
	return 0;
}

PopplerRectangle *_pdf_to_x(PopplerRectangle *pdf, double pdfw, double pdfh) {
	static PopplerRectangle x;
	if (!pdf) return &x;
	unsigned int ww, wh, ig;
	Window wig;
	XGetGeometry(dpy, presWin, &wig, &ig, &ig, &ww, &wh, &ig, &ig);
	x.x1 = ww * pdf->x1 / pdfw;
	x.x2 = ww * pdf->x2 / pdfw;
	x.y1 = wh - wh * pdf->y1 / pdfh;
	x.y2 = wh - wh * pdf->y2 / pdfh;
	double tmp;
	if (x.x1 > x.x2) { tmp = x.x1; x.x1 = x.x2; x.x2 = tmp; }
	if (x.y1 > x.y2) { tmp = x.y1; x.y1 = x.y2; x.y2 = tmp; }
	return &x;
}


char *_parse_fmt(const char *fmt, const char *fname) {
	static char line[256];
	char num[8];
	line[0] = '\0';
	const char *start, *end;
	PopplerRectangle *r = _pdf_to_x(NULL, 0, 0);
	for (start = fmt; *start;) {
		if ( (end=strchr(start, '%')) ) {
			strncat(line, start, end - start);
			if (*(++end) == 's') {
				if (fname) strcat(line, fname);
				else strcat(line, "%s");
			}
			else if (*end == 'x') {
				snprintf(num, 8, "%d", (int) r->x1);
				strcat(line, num);
			}
			else if (*end == 'y') {
				snprintf(num, 8, "%d", (int) r->y1);
				strcat(line, num);
			}
			else if (*end == 'w' || *end == 'W') {
				snprintf(num, 8, "%d", (int) (r->x2 -  r->x1));
				strcat(line, num);
			}
			else if (*end == 'h' || *end == 'H') {
				snprintf(num, 8, "%d", (int) (r->y2 - r->y1));
				strcat(line, num);
			}
			else if (*end == 'X') {
				snprintf(num, 8, "%d", (int) r->x1 + get_d(presX));
				strcat(line, num);
			}
			else if (*end == 'Y') {
				snprintf(num, 8, "%d", (int) r->y1 + get_d(presY));
				strcat(line, num);
			}
			else {
				num[0] = '%'; num[1] = *end; num[2] = '\0';
				strcat(line, num);
			}
			++end;
		}
		else strcat(line, start);
		start = end;
	}
	return line;
}

int _spawn(const char *fmt, const char *fname) {
	char *cmd = _parse_fmt(fmt, fname);
	system(cmd);
	printf("CMD: %s\n", cmd);
}

void _goto(PopplerAction *act){
	PopplerActionGotoDest *go = &act->goto_dest;
	if (go->dest->type == POPPLER_DEST_NAMED) {
		PopplerDest *d = poppler_document_find_dest(_pdf, go->dest->named_dest);
		cur = d->page_num - 1;
		poppler_dest_free(d);
	}
	else {
		cur = go->dest->page_num - 1;
	}
	command(cmdRedraw, NULL);
}

void _launch(PopplerAction *act){
	fprintf(stderr, "no link handler for launch actions\n");
	_none(act);
}

void _movie(PopplerAction *act){
	PopplerActionMovie *movie = &act->movie;
	if (movie->operation != POPPLER_ACTION_MOVIE_PLAY) {
		fprintf(stderr, "no link handler for movie controls\n");
		_none(act);
	}
	_spawn(get_s(linkMovie), poppler_movie_get_filename(movie->movie));
}

void _none(PopplerAction *act){
	fprintf(stderr, "Action link not handled: type=%d name=%s\n",
			act->type, act->any.title);
}

void _uri(PopplerAction *act){
	fprintf(stderr, "no link handler for uri actions\n");
	_none(act);
}

void _named(PopplerAction *act){
	PopplerActionNamed *name = &act->named;
	PopplerDest *d = poppler_document_find_dest(_pdf, name->named_dest);
	cur = d->page_num - 1;
	poppler_dest_free(d);
	command(cmdRedraw, NULL);
}

