/*************************************************************************************************
 * A sample searcher of Hyper Estraier
 *                                                      Copyright (C) 2004-2007 Mikio Hirabayashi
 * This file is part of Hyper Estraier.
 * Hyper Estraier is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Hyper Estraier is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Hyper
 * Estraier; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#if defined(MYFCGI)
#include <fcgi_stdio.h>
#endif
#include "estraier.h"
#include "myconf.h"

#define CONFSUFFIX     ".conf"           /* suffix of the configuration file */
#define DATTRLREAL     "_lreal"          /* name of the attribute of the real path */
#define DATTRLFILE     "_lfile"          /* name of the attribute of the file name */
#define DATTRSCORE     "#score"          /* name of the pseudo-attribute of score */
#define NUMBUFSIZ      32                /* size of a buffer for a number */
#define OUTBUFSIZ      262144            /* size of the output buffer */
#define MINIBNUM       31                /* bucket number of map for trivial use */
#define CONDATTRMAX    9                 /* maximum number of attribute conditions */
#define LOCKRETRYNUM   16                /* number of retries when locking failure */
#define MISSRETRYNUM   3                 /* number of retries when missing documents */
#define MISSINCRATIO   8                 /* ratio of increment number when missing */
#define NAVIPAGES      10                /* number of pages in paging navigation */
#define SPCACHEMNUM    1048576           /* max number of the special cache */

typedef struct {                         /* type of structure for a hitting object */
  const char *word;                      /* face of keyword */
  int pt;                                /* score tuned by TF-IDF */
} KEYSC;

enum {                                   /* enumeration for form type */
  FT_NORMAL = 0,                         /* normal */
  FT_WEB = 1,                            /* for web site */
  FT_FILE = 2,                           /* for file system */
  FT_MAIL = 3                            /* for mail box */
};

enum {                                   /* enumeration for navigation type */
  NM_NORMAL = 0,                         /* normal */
  NM_ADVANCED = 1,                       /* advanced */
  NM_HELP = 2                            /* help */
};


/* global variables for configurations */
const char *g_conffile = NULL;           /* path of the configuration file */
const char *g_indexname = NULL;          /* name of the index */
const char *g_tmplfile = NULL;           /* path of the template file */
const char *g_topfile = NULL;            /* path of the top page file */
const char *g_helpfile = NULL;           /* path of the help page file */
int g_lockindex = FALSE;                 /* whether to perform file locking to the database */
const CBLIST *g_replexprs = NULL;        /* list of URI replacement expressions */
int g_showlreal = FALSE;                 /* wether to show local real paths */
const char *g_deftitle = NULL;           /* default title string */
int g_formtype = FT_NORMAL;              /* form type */
const char *g_perpage = NULL;            /* parameters for the per-page select box */
int g_attrselect = FALSE;                /* whether to use select boxes for extension form */
const CBLIST *g_genrechecks = NULL;      /* list of checkboxes of genre attributes */
int g_attrwidth = -1;                    /* maximum width of each shown attribute */
int g_showscore = FALSE;                 /* whether to show scores */
const CBLIST *g_extattrs = NULL;         /* list of extra attributes of each document */
int g_snipwwidth = -1;                   /* whole width of the snippet */
int g_sniphwidth = -1;                   /* width of beginning of the text */
int g_snipawidth = -1;                   /* width around each highlighted word */
int g_condgstep = -1;                    /* step of N-gram */
int g_dotfidf = FALSE;                   /* whether to do TF-IDF tuning */
int g_scancheck = -1;                    /* number of checked documents by scanning */
int g_phraseform = 0;                    /* mode of phrase form */
const char *g_dispproxy = NULL;          /* proxy for marking display */
int g_candetail = FALSE;                 /* whether to show detail link */
int g_candir = FALSE;                    /* whether to show dir link */
int g_auxmin = -1;                       /* minimum hits to adopt the auxiliary index */
int g_smlrvnum = -1;                     /* number of elements of a vecter for similarity */
const char *g_smlrtune = NULL;           /* tuning parameters for similarity search */
int g_clipview = -1;                     /* number of clipped documents to be shown */
double g_clipweight = 0.0;               /* weighting algorithm of documents clipping */
int g_relkeynum = -1;                    /* number of related keywords to be shown */
const char *g_spcache = NULL;            /* name of the attribute of special cache */
int g_wildmax = -1;                      /* maximum number of extension of wild cards */
const char *g_qxpndcmd = NULL;           /* command for query expansion */
const char *g_logfile = NULL;            /* path of the log file */
const char *g_logformat = NULL;          /* format of the log */


/* global variables for parameters */
int p_navi = NM_NORMAL;                  /* navigation mode */
const char *p_phrase = NULL;             /* search phrase */
const char *p_attr = NULL;               /* narrowing attribute */
const char *p_attrval = NULL;            /* separated value of narrowing attribute */
const char *p_order = NULL;              /* ordering expression */
int p_perpage = 0;                       /* number of shown documents per page */
int p_clip = 0;                          /* lower limit of similarity eclipse */
int p_qxpnd = FALSE;                     /* whether to perform query expansion */
int p_gmasks = 0;                        /* masks for genre attributes */
int p_cinc = 0;                          /* ID of the parent of shown clipped documents */
int p_prec = FALSE;                      /* whether to search more precisely */
int p_pagenum = 0;                       /* number of the page */
int p_detail = 0;                        /* ID of the document to be detailed */
int p_similar = 0;                       /* ID of the seed document of similarity search */


/* other global variables */
char g_outbuf[OUTBUFSIZ];                /* output buffer */
const char *g_scriptname = NULL;         /* name of the script */
const char *g_tmpltext = NULL;           /* text of the template */
ESTDB *g_db = NULL;                      /* main database object */
double g_etime = 0.0;                    /* elepsed time */
int g_tabidx = 0;                        /* counter of tab indexes */
const CBLIST *g_attrlist = NULL;         /* list of advanced attributes */
int g_hnum = -1;                         /* number of corresponding documents */


/* function prototypes */
int main(int argc, char **argv);
static int realmain(int argc, char **argv);
static void showerror(const char *msg);
static const char *skiplabel(const char *str);
static CBMAP *getparameters(void);
static void myestdbclose(ESTDB *db);
static void xmlprintf(const char *format, ...);
static void setsimilarphrase(void);
static void showpage(void);
static void showtitle(void);
static void shownaviform(void);
static void showperpageform(const char *onchange);
static void showclipform(const char *onchange);
static void showgenreform(void);
static void showformnormal(void);
static void showformforweb(void);
static void showformforfile(void);
static void showformformail(void);
static void showtop(void);
static void expandquery(const char *word, CBLIST *result);
static int keysc_compare(const void *ap, const void *bp);
static void showresult(ESTDOC **docs, int dnum, CBMAP *hints, ESTCOND *cond, int hits, int miss,
                       KEYSC *scores, int scnum);
static void showdoc(ESTDOC *doc, const CBLIST *words, CBMAP *cnames, int detail,
                    const int *shadows, int snum, int *clipp);
static char *makeshownuri(const char *uri);
static void showinfo(void);
static void outputlog(void);


/* main routine */
int main(int argc, char **argv){
#if defined(MYFCGI)
  static int cnt = 0;
  est_proc_env_reset();
  while(FCGI_Accept() >= 0){
    p_navi = NM_NORMAL;
    p_phrase = NULL;
    p_attr = NULL;
    p_attrval = NULL;
    p_order = NULL;
    p_perpage = 0;
    p_clip = 0;
    p_qxpnd = FALSE;
    p_gmasks = 0;
    p_cinc = 0;
    p_prec = FALSE;
    p_pagenum = 0;
    p_detail = 0;
    p_similar = 0;
    g_attrlist = NULL;
    g_hnum = -1;
    realmain(argc, argv);
    fflush(stdout);
    if(++cnt >= 256) exit(0);
  }
  return 0;
#else
  est_proc_env_reset();
  return realmain(argc, argv);
#endif
}


/* real main routine */
static int realmain(int argc, char **argv){
  CBLIST *lines, *plist, *rlist, *glist, *alist, *vlist, *tlist;
  CBMAP *params;
  const char *rp;
  char *tmp, *wp, numbuf[NUMBUFSIZ];
  int i, len, ecode;
  /* set configurations */
  setvbuf(stdout, g_outbuf, _IOFBF, OUTBUFSIZ);
  g_scriptname = argv[0];
  if((rp = getenv("SCRIPT_NAME")) != NULL) g_scriptname = rp;
  if((rp = strrchr(g_scriptname, '/')) != NULL) g_scriptname = rp + 1;
  tmp = cbmalloc(strlen(g_scriptname) + strlen(CONFSUFFIX) + 1);
  sprintf(tmp, "%s", g_scriptname);
  cbglobalgc(tmp, free);
  if(!(wp = strrchr(tmp, '.'))) wp = tmp + strlen(tmp);
  sprintf(wp, "%s", CONFSUFFIX);
  g_conffile = tmp;
  if(!(lines = cbreadlines(g_conffile))) showerror("the configuration file is missing.");
  cbglobalgc(lines, (void (*)(void *))cblistclose);
  plist = cblistopen();
  cbglobalgc(plist, (void (*)(void *))cblistclose);
  rlist = cblistopen();
  cbglobalgc(rlist, (void (*)(void *))cblistclose);
  glist = cblistopen();
  cbglobalgc(glist, (void (*)(void *))cblistclose);
  alist = cblistopen();
  cbglobalgc(alist, (void (*)(void *))cblistclose);
  for(i = 0; i < cblistnum(lines); i++){
    rp = cblistval(lines, i, NULL);
    if(cbstrfwimatch(rp, "indexname:")){
      g_indexname = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "tmplfile:")){
      g_tmplfile = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "topfile:")){
      g_topfile = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "helpfile:")){
      g_helpfile = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "lockindex:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_lockindex = TRUE;
    } else if(cbstrfwimatch(rp, "pseudoindex:")){
      rp = skiplabel(rp);
      if(*rp != '\0') cblistpush(plist, rp, -1);
    } else if(cbstrfwimatch(rp, "replace:")){
      rp = skiplabel(rp);
      if(*rp != '\0') cblistpush(rlist, rp, -1);
    } else if(cbstrfwimatch(rp, "deftitle:")){
      g_deftitle = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "showlreal:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_showlreal = TRUE;
    } else if(cbstrfwimatch(rp, "formtype:")){
      rp = skiplabel(rp);
      if(!cbstricmp(rp, "web")){
        g_formtype = FT_WEB;
      } else if(!cbstricmp(rp, "file")){
        g_formtype = FT_FILE;
      } else if(!cbstricmp(rp, "mail")){
        g_formtype = FT_MAIL;
      } else {
        g_formtype = FT_NORMAL;
      }
    } else if(cbstrfwimatch(rp, "perpage:")){
      g_perpage = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "attrselect:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_attrselect = TRUE;
    } else if(cbstrfwimatch(rp, "genrecheck:")){
      rp = skiplabel(rp);
      if(*rp != '\0') cblistpush(glist, rp, -1);
    } else if(cbstrfwimatch(rp, "attrwidth:")){
      g_attrwidth = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "showscore:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_showscore = TRUE;
    } else if(cbstrfwimatch(rp, "extattr:")){
      rp = skiplabel(rp);
      if(*rp != '\0') cblistpush(alist, rp, -1);
    } else if(cbstrfwimatch(rp, "snipwwidth:")){
      g_snipwwidth = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "sniphwidth:")){
      g_sniphwidth = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "snipawidth:")){
      g_snipawidth = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "condgstep:")){
      g_condgstep = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "dotfidf:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_dotfidf = TRUE;
    } else if(cbstrfwimatch(rp, "scancheck:")){
      g_scancheck = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "phraseform:")){
      g_phraseform = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "dispproxy:")){
      g_dispproxy = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "candetail:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_candetail = TRUE;
    } else if(cbstrfwimatch(rp, "candir:")){
      if(!cbstricmp(skiplabel(rp), "true")) g_candir = TRUE;
    } else if(cbstrfwimatch(rp, "auxmin:")){
      g_auxmin = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "smlrvnum:")){
      g_smlrvnum = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "smlrtune:")){
      g_smlrtune = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "clipview:")){
      g_clipview = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "clipweight:")){
      rp = skiplabel(rp);
      if(!cbstricmp(rp, "url")) g_clipweight = ESTECLSIMURL;
    } else if(cbstrfwimatch(rp, "relkeynum:")){
      g_relkeynum = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "spcache:")){
      g_spcache = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "wildmax:")){
      g_wildmax = atoi(skiplabel(rp));
    } else if(cbstrfwimatch(rp, "qxpndcmd:")){
      g_qxpndcmd = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "logfile:")){
      g_logfile = skiplabel(rp);
    } else if(cbstrfwimatch(rp, "logformat:")){
      g_logformat = skiplabel(rp);
    }
  }
  if(!g_indexname) showerror("indexname is undefined.");
  if(!g_tmplfile) showerror("tmplfile is undefined.");
  if(!g_topfile) showerror("topfile is undefined.");
  if(!g_helpfile) showerror("helpfile is undefined.");
  g_replexprs = rlist;
  if(!g_deftitle) showerror("deftitle is undefined.");
  if(!g_perpage) showerror("perpage is undefined.");
  if(g_attrwidth < 0) showerror("attrwidth is undefined.");
  g_genrechecks = glist;
  g_extattrs = alist;
  if(g_snipwwidth < 0) showerror("snipwwidth is undefined.");
  if(g_sniphwidth < 0) showerror("sniphwidth is undefined.");
  if(g_snipawidth < 0) showerror("snipawidth is undefined.");
  if(g_condgstep < 1) showerror("condgstep is undefined.");
  if(g_scancheck < 0) showerror("scancheck is undefined.");
  if(g_phraseform < 1) showerror("phraseform is undefined.");
  if(!g_dispproxy) showerror("dispproxy is undefined.");
  if(!g_smlrtune) showerror("smlrtune is undefined.");
  if(!g_spcache) showerror("spcache is undefined.");
  if(g_wildmax < 0) showerror("wildmax is undefined.");
  if(!g_qxpndcmd) showerror("qxpndcmd is undefined.");
  if(!g_logfile) showerror("logfile is undefined.");
  if(!g_logformat) showerror("logformat is undefined.");
  /* read parameters */
  params = getparameters();
  cbglobalgc(params, (void (*)(void *))cbmapclose);
  if((rp = cbmapget(params, "navi", -1, NULL)) != NULL) p_navi = atoi(rp);
  if(!(p_phrase = cbmapget(params, "phrase", -1, NULL))) p_phrase = "";
  while(*p_phrase == ' ' || *p_phrase == '\t'){
    p_phrase++;
  }
  if(!(p_attr = cbmapget(params, "attr", -1, NULL))) p_attr = "";
  while(*p_attr == ' ' || *p_attr == '\t'){
    p_attr++;
  }
  if(!(p_attrval = cbmapget(params, "attrval", -1, NULL))) p_attrval = "";
  while(*p_attrval == ' ' || *p_attrval == '\t'){
    p_attrval++;
  }
  if(cbstrfwmatch(p_attr, "gstep=")){
    g_condgstep = atoi(p_attr + 6);
    p_attr = "";
  }
  if(cbstrfwmatch(p_attr, "tfidf=")){
    g_dotfidf = !cbstricmp(p_attr + 6, "true");
    p_attr = "";
  }
  if(cbstrfwmatch(p_attr, "scan=")){
    g_scancheck = atoi(p_attr + 5);
    p_attr = "";
  }
  if(!(p_order = cbmapget(params, "order", -1, NULL))) p_order = "";
  while(*p_order == ' ' || *p_order == '\t'){
    p_order++;
  }
  if((rp = cbmapget(params, "perpage", -1, NULL)) != NULL) p_perpage = atoi(rp);
  if(p_perpage < 1) p_perpage = atoi(g_perpage);
  if((rp = cbmapget(params, "clip", -1, NULL)) != NULL) p_clip = atoi(rp);
  if((rp = cbmapget(params, "qxpnd", -1, NULL)) != NULL) p_qxpnd = atoi(rp);
  for(i = 0; i < sizeof(int) * 8 - 1; i++){
    len = sprintf(numbuf, "genre%d", i + 1);
    if((rp = cbmapget(params, numbuf, len, NULL)) != NULL) p_gmasks |= 1 << i;
  }
  if((rp = cbmapget(params, "gmasks", -1, NULL)) != NULL) p_gmasks = atoi(rp);
  if(p_gmasks == 0) p_gmasks = -1;
  if((rp = cbmapget(params, "cinc", -1, NULL)) != NULL) p_cinc = atoi(rp);
  if((rp = cbmapget(params, "prec", -1, NULL)) != NULL) p_prec = atoi(rp) > 0;
  if((rp = cbmapget(params, "detail", -1, NULL)) != NULL) p_detail = atoi(rp);
  if(p_detail < 1) p_detail = 0;
  if((rp = cbmapget(params, "similar", -1, NULL)) != NULL) p_similar = atoi(rp);
  if(p_similar < 1) p_similar = 0;
  if((rp = cbmapget(params, "pagenum", -1, NULL)) != NULL) p_pagenum = atoi(rp);
  if(p_pagenum < 1) p_pagenum = 1;
  if((rp = cbmapget(params, "enc", -1, NULL)) != NULL){
    if((tmp = est_iconv(p_phrase, -1, rp, "UTF-8", NULL, NULL)) != NULL){
      p_phrase = tmp;
      cbglobalgc(tmp, free);
    }
    if((tmp = est_iconv(p_attr, -1, rp, "UTF-8", NULL, NULL)) != NULL){
      p_attr = tmp;
      cbglobalgc(tmp, free);
    }
    if((tmp = est_iconv(p_attrval, -1, rp, "UTF-8", NULL, NULL)) != NULL){
      p_attrval = tmp;
      cbglobalgc(tmp, free);
    }
    if((tmp = est_iconv(p_order, -1, rp, "UTF-8", NULL, NULL)) != NULL){
      p_order = tmp;
      cbglobalgc(tmp, free);
    }
  }
  if(p_navi == NM_ADVANCED){
    vlist = cblistopen();
    cbglobalgc(vlist, (void (*)(void *))cblistclose);
    for(i = 0; i <= CONDATTRMAX; i++){
      sprintf(numbuf, "attr%d", i);
      if((rp = cbmapget(params, numbuf, -1, NULL)) != NULL){
        while(*rp == ' ' || *rp == '\t'){
          rp++;
        }
        if(*rp != '\0') cblistpush(vlist, rp, -1);
      }
    }
    tlist = cbsplit(p_attr, -1, "\t");
    for(i = 0; i < cblistnum(tlist); i++){
      cblistpush(vlist, cblistval(tlist, i, NULL), -1);
    }
    cblistclose(tlist);
    g_attrlist = vlist;
  }
  /* read the other files and the database */
  if(!g_db){
    if(!(tmp = cbreadfile(g_tmplfile, NULL))) showerror("the template file is missing.");
    cbglobalgc(tmp, free);
    g_tmpltext = tmp;
    for(i = 0; i <= LOCKRETRYNUM; i++){
      if((g_db = est_db_open(g_indexname, ESTDBREADER | (g_lockindex ? ESTDBLCKNB : ESTDBNOLCK),
                             &ecode)) != NULL) break;
      if(ecode != ESTELOCK) showerror("the index is missing or broken.");
      est_usleep(1000 * 1000);
    }
    if(!g_db) showerror("the index is being updated now.");
    cbglobalgc(g_db, (void (*)(void *))myestdbclose);
    for(i = 0; i < cblistnum(plist); i++){
      est_db_add_pseudo_index(g_db, cblistval(plist, i, NULL));
    }
    if(g_spcache[0] != '\0') est_db_set_special_cache(g_db, g_spcache, SPCACHEMNUM);
    est_db_set_wildmax(g_db, g_wildmax);
  }
  if(p_phrase[0] == '\0' || p_detail > 0) g_scancheck = 0;
  setsimilarphrase();
  /* show the page */
  showpage();
  /* output the log message */
  outputlog();
  return 0;
}


/* show the error page and exit */
static void showerror(const char *msg){
  printf("Status: 500 Internal Server Error\r\n");
  printf("Content-Type: text/plain; charset=UTF-8\r\n");
  printf("\r\n");
  printf("Error: %s\n", msg);
  exit(1);
}


/* skip the label of a line */
static const char *skiplabel(const char *str){
  if(!(str = strchr(str, ':'))) return "";
  str++;
  while(*str != '\0' && (*str == ' ' || *str == '\t')){
    str++;
  }
  return str;
}


/* get CGI parameters */
static CBMAP *getparameters(void){
  int maxlen = 1024 * 1024 * 32;
  CBMAP *map, *attrs;
  CBLIST *pairs, *parts;
  const char *rp, *body;
  char *buf, *key, *val, *dkey, *dval, *wp, *bound, *fbuf, *aname;
  int i, len, c, blen, flen;
  map = cbmapopenex(37);
  buf = NULL;
  len = 0;
  if((rp = getenv("REQUEST_METHOD")) != NULL && !strcmp(rp, "POST") &&
     (rp = getenv("CONTENT_LENGTH")) != NULL && (len = atoi(rp)) > 0){
    if(len > maxlen) len = maxlen;
    buf = cbmalloc(len + 1);
    for(i = 0; i < len && (c = getchar()) != EOF; i++){
      buf[i] = c;
    }
    buf[i] = '\0';
    if(i != len){
      free(buf);
      buf = NULL;
    }
  } else if((rp = getenv("QUERY_STRING")) != NULL){
    buf = cbmemdup(rp, -1);
    len = strlen(buf);
  }
  if(buf && len > 0){
    if((rp = getenv("CONTENT_TYPE")) != NULL && cbstrfwmatch(rp, "multipart/form-data") &&
       (rp = strstr(rp, "boundary=")) != NULL){
      rp += 9;
      bound = cbmemdup(rp, -1);
      if((wp = strchr(bound, ';')) != NULL) *wp = '\0';
      parts = cbmimeparts(buf, len, bound);
      for(i = 0; i < cblistnum(parts); i++){
        body = cblistval(parts, i, &blen);
        attrs = cbmapopen();
        fbuf = cbmimebreak(body, blen, attrs, &flen);
        if((rp = cbmapget(attrs, "NAME", -1, NULL)) != NULL){
          cbmapput(map, rp, -1, fbuf, flen, FALSE);
          aname = cbsprintf("%s-filename", rp);
          if((rp = cbmapget(attrs, "FILENAME", -1, NULL)) != NULL)
            cbmapput(map, aname, -1, rp, -1, FALSE);
          free(aname);
        }
        free(fbuf);
        cbmapclose(attrs);
      }
      cblistclose(parts);
      free(bound);
    } else {
      pairs = cbsplit(buf, -1, "&");
      for(i = 0; i < cblistnum(pairs); i++){
        key = cbmemdup(cblistval(pairs, i, NULL), -1);
        if((val = strchr(key, '=')) != NULL){
          *(val++) = '\0';
          dkey = cburldecode(key, NULL);
          dval = cburldecode(val, NULL);
          cbmapput(map, dkey, -1, dval, -1, FALSE);
          free(dval);
          free(dkey);
        }
        free(key);
      }
      cblistclose(pairs);
    }
  }
  free(buf);
  return map;
}


/* close the database */
static void myestdbclose(ESTDB *db){
  int ecode;
  est_db_close(db, &ecode);
}


/* output escaped string */
static void xmlprintf(const char *format, ...){
  va_list ap;
  const char *rp;
  char *tmp, cbuf[32], *ebuf;
  unsigned char c;
  int cblen, cnt, mlen;
  va_start(ap, format);
  while(*format != '\0'){
    if(*format == '%'){
      cbuf[0] = '%';
      cblen = 1;
      format++;
      while(strchr("0123456789 .+-", *format) && *format != '\0' && cblen < sizeof(cbuf) - 1){
        cbuf[cblen++] = *format;
        format++;
      }
      cbuf[cblen++] = *format;
      cbuf[cblen] = '\0';
      switch(*format){
      case 's':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "(null)";
        printf(cbuf, tmp);
        break;
      case 'd':
        printf(cbuf, va_arg(ap, int));
        break;
      case 'o': case 'u': case 'x': case 'X': case 'c':
        printf(cbuf, va_arg(ap, unsigned int));
        break;
      case 'e': case 'E': case 'f': case 'g': case 'G':
        printf(cbuf, va_arg(ap, double));
        break;
      case '@':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "(null)";
        ebuf = NULL;
        if(cblen > 2){
          mlen = atoi(cbuf + 1) * 10;
          cnt = 0;
          rp = tmp;
          while(*rp != '\0'){
            if((*rp & 0x80) == 0x00){
              cnt += 10;
            } else if((*rp & 0xe0) == 0xc0){
              cnt += 15;
            } else if((*rp & 0xf0) == 0xe0 || (*rp & 0xf8) == 0xf0){
              cnt += 20;
            }
            if(cnt > mlen){
              ebuf = cbmemdup(tmp, rp - tmp);
              tmp = ebuf;
              break;
            }
            rp++;
          }
        }
        while(*tmp){
          switch(*tmp){
          case '&': printf("&amp;"); break;
          case '<': printf("&lt;"); break;
          case '>': printf("&gt;"); break;
          case '"': printf("&quot;"); break;
          default:
            if(!((*tmp >= 0 && *tmp <= 0x8) || (*tmp >= 0x0e && *tmp <= 0x1f))) putchar(*tmp);
            break;
            }
          tmp++;
        }
        if(ebuf){
          free(ebuf);
          printf("...");
        }
        break;
      case '?':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "(null)";
        while(*tmp){
          c = *(unsigned char *)tmp;
          if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || (c != '\0' && strchr("_-.", c))){
            putchar(c);
          } else {
            printf("%%%02X", c);
          }
          tmp++;
        }
        break;
      case '$':
        tmp = va_arg(ap, char *);
        if(!tmp) tmp = "null";
        while(*tmp){
          c = *(unsigned char *)tmp;
          if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            putchar(c);
          tmp++;
        }
        break;
      case '%':
        putchar('%');
        break;
      }
    } else {
      putchar(*format);
    }
    format++;
  }
  va_end(ap);
}


/* set the phrase for similarity search */
static void setsimilarphrase(void){
  ESTDOC *doc;
  CBMAP *svmap;
  CBDATUM *datum;
  const char *kbuf, *vbuf;
  char *ptr;
  int ksiz, vsiz;
  if(!cbstrfwimatch(p_phrase, ESTOPSIMILAR) && p_similar < 1) return;
  g_scancheck = 0;
  if(g_smlrvnum < 1){
    p_phrase = "";
    return;
  }
  if(p_similar < 1) return;
  svmap = est_db_get_keywords(g_db, p_similar);
  if(!svmap && (doc = est_db_get_doc(g_db, p_similar, 0)) != NULL){
    svmap = est_db_etch_doc(g_dotfidf ? g_db : NULL, doc, g_smlrvnum);
    est_doc_delete(doc);
  } else if(!svmap){
    return;
  }
  datum = cbdatumopen(ESTOPSIMILAR " ", -1);
  cbdatumcat(datum, g_smlrtune, -1);
  cbmapiterinit(svmap);
  while((kbuf = cbmapiternext(svmap, &ksiz)) != NULL){
    vbuf = cbmapget(svmap, kbuf, ksiz, &vsiz);
    cbdatumcat(datum, " WITH ", -1);
    cbdatumcat(datum, vbuf, vsiz);
    cbdatumcat(datum, " ", 1);
    cbdatumcat(datum, kbuf, ksiz);
  }
  ptr = cbdatumtomalloc(datum, NULL);
  cbglobalgc(ptr, free);
  p_phrase = ptr;
  cbmapclose(svmap);
}


/* show the page */
static void showpage(void){
  KEYSC *scores;
  ESTCOND *cond;
  ESTDOC **docs;
  CBMAP *hints, *allkwd, *dockwd;
  CBLIST *elems;
  CBDATUM *datum;
  const char *rp, *pv, *vbuf;
  char *tmp, numbuf[NUMBUFSIZ];
  int i, tnum, max, *res, rnum, hits, sc, dnum, miss, check, len, scnum;
  printf("Cache-Control: no-cache, must-revalidate, no-transform\r\n");
  printf("Pragma: no-cache\r\n");
  printf("Content-Disposition: inline; filename=%s\r\n", g_scriptname);
  printf("Content-Type: text/html; charset=UTF-8\r\n");
  printf("\r\n");
  g_etime = est_gettimeofday();
  g_tabidx = 0;
  cond = est_cond_new();
  if(g_qxpndcmd[0] != '\0' && p_qxpnd) est_cond_set_expander(cond, expandquery);
  if(!strcmp(p_attr, "_fpath_")){
    if(p_phrase[0] != '\0'){
      tmp = cbsprintf(DATTRLREAL " ISTRINC %s", p_phrase);
      est_cond_add_attr(cond, tmp);
      free(tmp);
    }
    g_scancheck = 0;
  } else if(!strcmp(p_attr, "_subj_")){
    if(p_phrase[0] != '\0'){
      tmp = cbsprintf("subject ISTRINC %s", p_phrase);
      est_cond_add_attr(cond, tmp);
      free(tmp);
    }
    g_scancheck = 0;
  } else if(!strcmp(p_attr, "_from_")){
    if(p_phrase[0] != '\0'){
      tmp = cbsprintf("from ISTRINC %s", p_phrase);
      est_cond_add_attr(cond, tmp);
      free(tmp);
    }
    g_scancheck = 0;
  } else if(!strcmp(p_attr, "_toc_")){
    if(p_phrase[0] != '\0'){
      tmp = cbsprintf("to,cc ISTRINC %s", p_phrase);
      est_cond_add_attr(cond, tmp);
      free(tmp);
    }
    g_scancheck = 0;
  } else if(!strcmp(p_attr, "_titleB_")){
    if(p_phrase[0] != '\0'){
      tmp = cbsprintf("@title ISTRBW %s", p_phrase);
      est_cond_add_attr(cond, tmp);
      free(tmp);
    }
    g_scancheck = 0;
  } else if(!strcmp(p_attr, "_titleI_")){
    if(p_phrase[0] != '\0'){
      tmp = cbsprintf("@title ISTRINC %s", p_phrase);
      est_cond_add_attr(cond, tmp);
      free(tmp);
    }
    g_scancheck = 0;
  } else {
    if(p_phrase[0] != '\0') est_cond_set_phrase(cond, p_phrase);
    if(g_attrlist){
      for(i = 0; i < cblistnum(g_attrlist); i++){
        est_cond_add_attr(cond, cblistval(g_attrlist, i, NULL));
      }
    } else if(p_attr[0] != '\0'){
      if(p_attrval[0] != '\0'){
        tmp = cbsprintf("%s %s", p_attr, p_attrval);
        est_cond_add_attr(cond, tmp);
        free(tmp);
      } else {
        est_cond_add_attr(cond, p_attr);
      }
    }
  }
  if(cblistnum(g_genrechecks) > 0){
    for(i = 0; i < cblistnum(g_genrechecks); i++){
      if(!(p_gmasks & (1 << i))){
        datum = cbdatumopen("@genre STROREQ", -1);
        for(i = 0; i < cblistnum(g_genrechecks); i++){
          if(p_gmasks & (1 << i)){
            rp = cblistval(g_genrechecks, i, NULL);
            cbdatumcat(datum, " ", 1);
            if((pv = strstr(rp, "{{!}}")) != NULL){
              cbdatumcat(datum, rp, pv - rp);
            } else {
              cbdatumcat(datum, rp, -1);
            }
          }
        }
        est_cond_add_attr(cond, cbdatumptr(datum));
        cbdatumclose(datum);
        break;
      }
    }
  }
  if(!strcmp(p_order, "_date_")){
    est_cond_set_order(cond, ESTDATTRMDATE " " ESTORDNUMD);
  } else if(!strcmp(p_order, "_size_")){
    est_cond_set_order(cond, ESTDATTRSIZE " " ESTORDNUMD);
  } else if(p_order[0] != '\0'){
    est_cond_set_order(cond, p_order);
  }
  switch(g_condgstep){
  case 1:
    est_cond_set_options(cond, ESTCONDSURE);
    break;
  case 2:
    est_cond_set_options(cond, ESTCONDUSUAL);
    break;
  case 3:
    est_cond_set_options(cond, ESTCONDFAST);
    break;
  case 4:
    est_cond_set_options(cond, ESTCONDAGITO);
    break;
  }
  if(!g_dotfidf) est_cond_set_options(cond, ESTCONDNOIDF);
  switch(g_phraseform){
  case 2:
    est_cond_set_options(cond, ESTCONDSIMPLE);
    break;
  case 3:
    est_cond_set_options(cond, ESTCONDROUGH);
    break;
  case 4:
    est_cond_set_options(cond, ESTCONDUNION);
    break;
  case 5:
    est_cond_set_options(cond, ESTCONDISECT);
    break;
  }
  if(g_showscore) est_cond_set_options(cond, ESTCONDSCFB);
  est_cond_set_auxiliary(cond, p_prec ? -1 :
                         (g_auxmin > p_perpage * 1.3 + 1 ? g_auxmin : p_perpage * 1.3 + 1));
  if(p_clip > 0) est_cond_set_eclipse(cond, p_clip / 10.0 + (p_clip <= 10 ? g_clipweight: 0));
  tnum = 0;
  max = p_pagenum * p_perpage * 1.3 + 1;
  hits = 0;
  do {
    est_cond_set_max(cond, max);
    hints = cbmapopenex(MINIBNUM);
    res = est_db_search(g_db, cond, &rnum, hints);
    hits = (rp = cbmapget(hints, "", 0, NULL)) ? atoi(rp) : rnum;
    if(g_candetail && p_detail > 0){
      if(rnum < 1) cbmapput(hints, "", 0, "1", 1, TRUE);
      free(res);
      res = cbmalloc(sizeof(int));
      res[0] = p_detail;
      rnum = 1;
    }
    docs = cbmalloc(rnum * sizeof(ESTDOC *) + 1);
    dnum = 0;
    miss = 0;
    check = p_phrase[0] == '\0' || p_phrase[0] == '[' || p_phrase[0] == '*' ? 0 : g_scancheck;
    for(i = 0; i < rnum; i++){
      docs[dnum] = est_db_get_doc(g_db, res[i],
                                  dnum < p_pagenum * p_perpage || check > 0 ? 0 :
                                  ESTGDNOATTR | ESTGDNOTEXT | ESTGDNOKWD);
      if(!docs[dnum]){
        miss++;
        continue;
      }
      if(check > 0 && !est_db_scan_doc(g_db, docs[dnum], cond)){
        est_doc_delete(docs[dnum]);
        miss++;
        continue;
      }
      if((sc = est_cond_score(cond, i)) >= 0){
        sprintf(numbuf, "%d", sc);
        est_doc_add_attr(docs[dnum], DATTRSCORE, numbuf);
      }
      dnum++;
      check--;
    }
    if((tnum <= MISSRETRYNUM && miss > 0 && max <= rnum && dnum < p_pagenum * p_perpage + 1) ||
       (p_pagenum == 1 && tnum == 0 && (hits < g_auxmin || hits < p_perpage) &&
        est_cond_auxiliary_word(cond, ""))){
      for(i = 0; i < dnum; i++){
        est_doc_delete(docs[i]);
      }
      free(docs);
      free(res);
      cbmapclose(hints);
      max *= MISSINCRATIO;
      if(p_pagenum == 1) est_cond_set_auxiliary(cond, -1);
      tnum++;
      continue;
    }
    break;
  } while(TRUE);
  if(g_relkeynum > 0){
    allkwd = cbmapopenex(MINIBNUM);
    for(i = 0; i < dnum; i++){
      if(!(dockwd = est_doc_keywords(docs[i]))) continue;
      cbmapiterinit(dockwd);
      while((rp = cbmapiternext(dockwd, &len)) != NULL){
        sc = ((vbuf = cbmapget(allkwd, rp, len, NULL)) != NULL ? atoi(vbuf) : 0) +
          pow((atoi(cbmapget(dockwd, rp, len, NULL)) + 100) * 10, 0.7);
        sprintf(numbuf, "%d", sc);
        cbmapput(allkwd, rp, len, numbuf, -1, TRUE);
      }
    }
    scores = cbmalloc(cbmaprnum(allkwd) * sizeof(KEYSC) + 1);
    scnum = 0;
    cbmapiterinit(allkwd);
    while((rp = cbmapiternext(allkwd, &len)) != NULL){
      scores[scnum].word = rp;
      scores[scnum].pt = atoi(cbmapget(allkwd, rp, len, NULL));
      scnum++;
    }
    qsort(scores, scnum, sizeof(KEYSC), keysc_compare);
  } else {
    allkwd = NULL;
    scores = NULL;
    scnum = 0;
  }
  g_etime = est_gettimeofday() - g_etime;
  elems = cbxmlbreak(g_tmpltext, FALSE);
  for(i = 0; i < cblistnum(elems); i++){
    rp = cblistval(elems, i, NULL);
    if(!strcmp(rp, "<!--ESTTITLE-->")){
      showtitle();
    } else if(!strcmp(rp, "<!--ESTFORM-->")){
      switch(g_formtype){
      default:
        showformnormal();
        break;
      case FT_WEB:
        showformforweb();
        break;
      case FT_FILE:
        showformforfile();
        break;
      case FT_MAIL:
        showformformail();
        break;
      }
    } else if(!strcmp(rp, "<!--ESTRESULT-->")){
      if(p_phrase[0] == '\0' && p_attr[0] == '\0' && p_detail < 1){
        showtop();
      } else {
        showresult(docs, dnum, hints, cond, hits, miss, scores, scnum);
      }
    } else if(!strcmp(rp, "<!--ESTINFO-->")){
      showinfo();
    } else {
      printf("%s", rp);
    }
  }
  for(i = 0; i < dnum; i++){
    est_doc_delete(docs[i]);
  }
  cblistclose(elems);
  if(scores) free(scores);
  if(allkwd) cbmapclose(allkwd);
  free(docs);
  free(res);
  cbmapclose(hints);
  est_cond_delete(cond);
}


/* show the page title */
static void showtitle(void){
  if(*p_phrase != '\0'){
    xmlprintf("Search Result: %48@", p_phrase);
  } else if(*p_attr != '\0'){
    xmlprintf("Search Result: %48@", p_attr);
  } else {
    xmlprintf("%@", g_deftitle);
  }
}


/* show the navigation form */
static void shownaviform(void){
  xmlprintf("<div class=\"form_navi\">\n");
  if(p_navi == NM_ADVANCED){
    xmlprintf("<span class=\"navivoid\">advanced</span>\n");
  } else {
    xmlprintf("<a href=\"%@?navi=%d\" class=\"navilink\">advanced</a>\n",
              g_scriptname, NM_ADVANCED);
  }
  if(p_navi == NM_HELP){
    xmlprintf("<span class=\"navivoid\">help</span>\n");
  } else {
    xmlprintf("<a href=\"%@?navi=%d\" class=\"navilink\">help</a>\n",
              g_scriptname, NM_HELP);
  }
  xmlprintf("</div>\n");
}


/* show the per page form */
static void showperpageform(const char *onchange){
  const char *rp;
  int i, start, end, step;
  start = atoi(g_perpage);
  if((rp = strchr(g_perpage, ' ')) != NULL || (rp = strchr(g_perpage, '\t')) != NULL){
    while(*rp == ' ' || *rp == '\t'){
      rp++;
    }
    end = atoi(rp);
    if((rp = strchr(rp, ' ')) != NULL || (rp = strchr(rp, '\t')) != NULL){
      while(*rp == ' ' || *rp == '\t'){
        rp++;
      }
      step = atoi(rp);
    } else {
      step = start;
    }
  } else {
    end = start * 10;
    step = start;
  }
  xmlprintf("<select name=\"perpage\"");
  if(onchange) xmlprintf(" onchange=\"%@\"", onchange);
  xmlprintf(" id=\"perpage\" tabindex=\"%d\">\n", ++g_tabidx);
  for(i = start; i <= end; i += step){
    xmlprintf("<option value=\"%d\"%s>%d</option>\n",
              i, i == p_perpage ? " selected=\"selected\"" : "", i);
  }
  xmlprintf("</select>\n");
}


/* show the clip form */
static void showclipform(const char *onchange){
  int i;
  xmlprintf("<select name=\"clip\"");
  if(onchange) xmlprintf(" onchange=\"%@\"", onchange);
  xmlprintf(" id=\"unlike\" tabindex=\"%d\">\n", ++g_tabidx);
  xmlprintf("<option value=\"-1\">--</option>\n");
  for(i = 9; i > 0; i--){
    xmlprintf("<option value=\"%d\"%s>0.%d</option>\n",
              i, i == p_clip ? " selected=\"selected\"" : "", i);
  }
  xmlprintf("<option value=\"%d\"%s>file</option>\n", (int)(ESTECLFILE * 10),
            p_clip == (int)(ESTECLFILE * 10) ? " selected=\"selected\"" : "", i);
  xmlprintf("<option value=\"%d\"%s>dir</option>\n", (int)(ESTECLDIR * 10),
            p_clip == (int)(ESTECLDIR * 10) ? " selected=\"selected\"" : "", i);
  xmlprintf("<option value=\"%d\"%s>serv</option>\n", (int)(ESTECLSERV * 10),
            p_clip == (int)(ESTECLSERV * 10) ? " selected=\"selected\"" : "", i);
  xmlprintf("</select>\n");
}


/* show the genre form */
static void showgenreform(void){
  const char *rp, *pv;
  int i;
  for(i = 0; i < cblistnum(g_genrechecks); i++){
    rp = cblistval(g_genrechecks, i, NULL);
    if((pv = strstr(rp, "{{!}}")) != NULL) rp = pv + 5;
    xmlprintf("<input type=\"checkbox\" name=\"genre%d\" value=\"on\"%s id=\"genre%d\""
              " class=\"checkbox\" tabindex=\"%d\" accesskey=\"5\" />",
              i + 1, p_gmasks & (1 << i) ? " checked=\"checked\"" : "", i + 1, ++g_tabidx);
    xmlprintf("<label for=\"genre%d\" class=\"genrecheck\">:%s</label>\n", i + 1, rp);
  }
}


/* show the advanced form */
static void showadvancedform(void){
  const char *rp;
  int i;
  xmlprintf("<div class=\"form_advanced\">\n");
  xmlprintf("<table summary=\"condition\" class=\"condition\">\n");
  xmlprintf("<tr>\n");
  xmlprintf("<th class=\"ilabel\" abbr=\"phrase\">phrase:</th>\n");
  xmlprintf("<td class=\"ivalue\">\n");
  xmlprintf("<input type=\"text\" name=\"phrase\" value=\"%@\""
            " size=\"64\" id=\"phrase\" class=\"text\" tabindex=\"%d\" accesskey=\"0\" />\n",
            p_phrase, ++g_tabidx);
  xmlprintf("</td>\n");
  xmlprintf("</tr>\n");
  for(i = 0; i < 3; i++){
    xmlprintf("<tr>\n");
    xmlprintf("<th class=\"ilabel\" abbr=\"attribute\">attribute:</th>\n");
    xmlprintf("<td class=\"ivalue\">\n");
    if(!(rp = cblistval(g_attrlist, i, NULL))) rp = "";
    xmlprintf("<input type=\"text\" name=\"attr%d\" value=\"%@\""
              " size=\"64\" id=\"attr%d\" class=\"text\" tabindex=\"%d\" accesskey=\"%d\" />\n",
              i, rp, i, ++g_tabidx, i);
    xmlprintf("</td>\n");
    xmlprintf("</tr>\n");
  }
  xmlprintf("<tr>\n");
  xmlprintf("<th class=\"ilabel\" abbr=\"order\">order:</th>\n");
  xmlprintf("<td class=\"ivalue\">\n");
  xmlprintf("<input type=\"text\" name=\"order\" value=\"%@\""
            " size=\"64\" id=\"order\" class=\"text\" tabindex=\"%d\" accesskey=\"0\" />\n",
            p_order, ++g_tabidx);
  xmlprintf("</td>\n");
  xmlprintf("</tr>\n");
  if(cblistnum(g_genrechecks) > 0){
    xmlprintf("<tr>\n");
    xmlprintf("<th class=\"ilabel\" abbr=\"genre\">genre:</th>\n");
    xmlprintf("<td class=\"ivalue\">\n");
    showgenreform();
    xmlprintf("</td>\n");
    xmlprintf("</tr>\n");
  }
  xmlprintf("<tr>\n");
  xmlprintf("<th class=\"ilabel\" abbr=\"perpage\">per page:</th>\n");
  xmlprintf("<td class=\"ivalue\">\n");
  showperpageform(NULL);
  xmlprintf("</td>\n");
  xmlprintf("</tr>\n");
  xmlprintf("<tr>\n");
  xmlprintf("<th class=\"ilabel\" abbr=\"clip\">clip:</th>\n");
  xmlprintf("<td class=\"ivalue\">\n");
  showclipform(NULL);
  xmlprintf("</td>\n");
  xmlprintf("</tr>\n");
  xmlprintf("<tr>\n");
  xmlprintf("<th class=\"ilabel\" abbr=\"phrase\">action:</th>\n");
  xmlprintf("<td class=\"ivalue\">\n");
  xmlprintf("<input type=\"submit\" value=\"Search\""
            " id=\"search\" class=\"submit\" tabindex=\"%d\" accesskey=\"8\" />\n",
            ++g_tabidx);
  xmlprintf("<input type=\"reset\" value=\"Reset\""
            " id=\"reset\" class=\"reset\" tabindex=\"%d\" accesskey=\"9\" />\n",
            ++g_tabidx);
  xmlprintf("</td>\n");
  xmlprintf("</tr>\n");
  xmlprintf("</table>\n");
  xmlprintf("<span class=\"inputhidden\">\n");
  xmlprintf("<input type=\"hidden\" name=\"navi\" value=\"%d\" id=\"navi\" />\n", p_navi);
  xmlprintf("</span>\n");
  xmlprintf("</div>\n");
}


/* show the normal form */
static void showformnormal(void){
  xmlprintf("<div id=\"estform\" class=\"estform\">\n");
  xmlprintf("<form action=\"%@\" method=\"get\" id=\"form_self\">\n", g_scriptname);
  shownaviform();
  if(p_navi == NM_ADVANCED){
    showadvancedform();
  } else {
    xmlprintf("<div class=\"form_basic\">\n");
    xmlprintf("<input type=\"text\" name=\"phrase\" value=\"%@\""
              " size=\"80\" id=\"phrase\" class=\"text\" tabindex=\"%d\" accesskey=\"0\" />\n",
              p_phrase, ++g_tabidx);
    xmlprintf("<input type=\"submit\" value=\"Search\""
              " id=\"search\" class=\"submit\" tabindex=\"%d\" accesskey=\"1\" />\n",
              ++g_tabidx);
    xmlprintf("</div>\n");
    xmlprintf("<div class=\"form_extension\">\n");
    showperpageform(NULL);
    xmlprintf("per page,\n");
    if(g_attrselect){
      xmlprintf("with\n");
      xmlprintf("<select name=\"attr\" id=\"attr\" tabindex=\"%d\">\n", ++g_tabidx);
      xmlprintf("<option value=\"\">--</option>\n");
      xmlprintf("<option value=\"@title ISTRINC\"%s>title including</option>\n",
                cbstrfwmatch(p_attr, "@title ISTRINC") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@title ISTRBW\"%s>title beginning with</option>\n",
                cbstrfwmatch(p_attr, "@title ISTRBW") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@title ISTREW\"%s>title ending with</option>\n",
                cbstrfwmatch(p_attr, "@title ISTREW") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@author ISTRINC\"%s>author including</option>\n",
                cbstrfwmatch(p_attr, "@author ISTRINC") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@author ISTRBW\"%s>author beginning with</option>\n",
                cbstrfwmatch(p_attr, "@author ISTRBW") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@author ISTREW\"%s>author ending with</option>\n",
                cbstrfwmatch(p_attr, "@author ISTREW") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@mdate NUMLT\"%s>date less than</option>\n",
                cbstrfwmatch(p_attr, "@mdate NUMLT") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@mdate NUMGE\"%s>date not less than</option>\n",
                cbstrfwmatch(p_attr, "@mdate NUMGE") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@size NUMLT\"%s>size less than</option>\n",
                cbstrfwmatch(p_attr, "@size NUMLT") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@size NUMGE\"%s>size not less than</option>\n",
                cbstrfwmatch(p_attr, "@size NUMGE") ? " selected=\"selected\"" : "");
      xmlprintf("</select>\n");
      xmlprintf("<input type=\"text\" name=\"attrval\" value=\"%@\""
                " size=\"12\" id=\"attrval\" class=\"text\" tabindex=\"%d\" accesskey=\"2\" />\n",
                p_attrval, ++g_tabidx);
      xmlprintf(", order by\n");
      xmlprintf("<select name=\"order\" id=\"order\" tabindex=\"%d\">\n", ++g_tabidx);
      xmlprintf("<option value=\"\">score</option>\n");
      xmlprintf("<option value=\"@title STRA\"%s>title (asc)</option>\n",
                !strcmp(p_order, "@title STRA") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@title STRD\"%s>title (desc)</option>\n",
                !strcmp(p_order, "@title STRD") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@author STRA\"%s>author (asc)</option>\n",
                !strcmp(p_order, "@author STRA") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@author STRD\"%s>author (desc)</option>\n",
                !strcmp(p_order, "@author STRD") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@mdate NUMA\"%s>date (asc)</option>\n",
                !strcmp(p_order, "@mdate NUMA") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@mdate NUMD\"%s>date (desc)</option>\n",
                !strcmp(p_order, "@mdate NUMD") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@size NUMA\"%s>size (asc)</option>\n",
                !strcmp(p_order, "@size NUMA") ? " selected=\"selected\"" : "");
      xmlprintf("<option value=\"@size NUMD\"%s>size (desc)</option>\n",
                !strcmp(p_order, "@size NUMD") ? " selected=\"selected\"" : "");
      xmlprintf("</select>\n");
    } else {
      xmlprintf("with\n");
      xmlprintf("<input type=\"text\" name=\"attr\" value=\"%@\""
                " size=\"18\" id=\"attr\" class=\"text\" tabindex=\"%d\" accesskey=\"2\" />\n",
                p_attr, ++g_tabidx);
      xmlprintf(", order by\n");
      xmlprintf("<input type=\"text\" name=\"order\" value=\"%@\""
                " size=\"14\" id=\"order\" class=\"text\" tabindex=\"%d\" accesskey=\"3\" />\n",
                p_order, ++g_tabidx);
    }
    if(g_clipview >= 0){
      xmlprintf(", clip by\n");
      showclipform(NULL);
    }
    if(g_qxpndcmd[0] != '\0'){
      xmlprintf(", expansion:\n");
      xmlprintf("<input type=\"checkbox\" name=\"qxpnd\" value=\"1\"%s id=\"qxpnd\""
                " class=\"checkbox\" tabindex=\"%d\" accesskey=\"4\" />\n",
                p_qxpnd ? " checked=\"checked\"" : "", ++g_tabidx);
    }
    xmlprintf("<span class=\"inputhidden\">\n");
    xmlprintf("<input type=\"hidden\" name=\"navi\" value=\"%d\" id=\"navi\" />\n", p_navi);
    xmlprintf("</span>\n");
    xmlprintf("</div>\n");
    if(cblistnum(g_genrechecks) > 0){
      xmlprintf("<div class=\"form_genrecheck\">\n");
      showgenreform();
      xmlprintf("</div>\n");
    }
  }
  xmlprintf("</form>\n");
  xmlprintf("</div>\n");
}


/* show the form for web site */
static void showformforweb(void){
  xmlprintf("<div id=\"estform\" class=\"estform\">\n");
  xmlprintf("<form action=\"%@\" method=\"get\" id=\"form_self\">\n", g_scriptname);
  shownaviform();
  if(p_navi == NM_ADVANCED){
    showadvancedform();
  } else {
    xmlprintf("<div class=\"form_basic\">\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("phrase:\n");
    xmlprintf("<input type=\"text\" name=\"phrase\" value=\"%@\""
              " size=\"52\" id=\"phrase\" class=\"text\" tabindex=\"%d\" accesskey=\"0\" />\n",
              p_phrase, ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("max:\n");
    showperpageform("changemax();");
    xmlprintf("</span>\n");
    if(g_clipview >= 0){
      xmlprintf("<span class=\"inputunit\">\n");
      xmlprintf("clip:\n");
      showclipform("changeclip();");
      xmlprintf("</span>\n");
    }
    xmlprintf("</div>\n");
    xmlprintf("<div class=\"form_extension\">\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("target:\n");
    xmlprintf("<input type=\"button\" value=\"body text\""
              " onclick=\"changetarget('');\" onkeypress=\"changetarget('');\""
              " class=\"button wbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_attr[0] == '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"title (begin)\""
              " onclick=\"changetarget('_titleB_');\" onkeypress=\"changetarget('_titleB_');\""
              " class=\"button wbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_attr, "_titleB_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"title (include)\""
              " onclick=\"changetarget('_titleI_');\" onkeypress=\"changetarget('_titleI_');\""
              " class=\"button wbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_attr, "_titleI_")  ? " abutton" : "", ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("order:\n");
    xmlprintf("<input type=\"button\" value=\"score\""
              " onclick=\"changeorder('');\" onkeypress=\"chaneorder('');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_order[0] == '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"date\""
              " onclick=\"changeorder('_date_');\" onkeypress=\"chaneorder('_date_');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_order, "_date_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"size\""
              " onclick=\"changeorder('_size_');\" onkeypress=\"chaneorder('_size_');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_order, "_size_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputhidden\">\n");
    xmlprintf("<input type=\"hidden\" name=\"navi\" value=\"%d\" id=\"navi\" />\n", p_navi);
    xmlprintf("<input type=\"hidden\" name=\"attr\" value=\"%@\" id=\"attr\" />\n", p_attr);
    xmlprintf("<input type=\"hidden\" name=\"order\" value=\"%@\" id=\"order\" />\n", p_order);
    xmlprintf("</span>\n");
    xmlprintf("</div>\n");
    if(cblistnum(g_genrechecks) > 0){
      xmlprintf("<div class=\"form_genrecheck\">\n");
      xmlprintf("<span class=\"inputunit\">\n");
      xmlprintf("genre:\n");
      showgenreform();
      xmlprintf("</span>\n");
      xmlprintf("</div>\n");
    }
  }
  xmlprintf("</form>\n");
  xmlprintf("</div>\n");
}


/* show the form for file system */
static void showformforfile(void){
  xmlprintf("<div id=\"estform\" class=\"estform\">\n");
  xmlprintf("<form action=\"%@\" method=\"get\" id=\"form_self\">\n", g_scriptname);
  shownaviform();
  if(p_navi == NM_ADVANCED){
    showadvancedform();
  } else {
    xmlprintf("<div class=\"form_basic\">\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("phrase:\n");
    xmlprintf("<input type=\"text\" name=\"phrase\" value=\"%@\""
              " size=\"32\" id=\"phrase\" class=\"text\" tabindex=\"%d\" accesskey=\"0\" />\n",
              p_phrase, ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("max:\n");
    showperpageform("changemax();");
    xmlprintf("</span>\n");
    if(g_clipview >= 0){
      xmlprintf("<span class=\"inputunit\">\n");
      xmlprintf("clip:\n");
      showclipform("changeclip();");
      xmlprintf("</span>\n");
    }
    xmlprintf("</div>\n");
    xmlprintf("<div class=\"form_extension\">\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("target:\n");
    xmlprintf("<input type=\"button\" value=\"body text\""
              " onclick=\"changetarget('');\" onkeypress=\"changetarget('');\""
              " class=\"button wbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_attr[0] == '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"file path\""
              " onclick=\"changetarget('_fpath_');\" onkeypress=\"changetarget('_fpath_');\""
              " class=\"button wbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_attr[0] != '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("order:\n");
    xmlprintf("<input type=\"button\" value=\"score\""
              " onclick=\"changeorder('');\" onkeypress=\"chaneorder('');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_order[0] == '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"date\""
              " onclick=\"changeorder('_date_');\" onkeypress=\"chaneorder('_date_');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_order, "_date_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"size\""
              " onclick=\"changeorder('_size_');\" onkeypress=\"chaneorder('_size_');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_order, "_size_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputhidden\">\n");
    xmlprintf("<input type=\"hidden\" name=\"navi\" value=\"%d\" id=\"navi\" />\n", p_navi);
    xmlprintf("<input type=\"hidden\" name=\"attr\" value=\"%@\" id=\"attr\" />\n", p_attr);
    xmlprintf("<input type=\"hidden\" name=\"order\" value=\"%@\" id=\"order\" />\n", p_order);
    xmlprintf("</span>\n");
    xmlprintf("</div>\n");
    if(cblistnum(g_genrechecks) > 0){
      xmlprintf("<div class=\"form_genrecheck\">\n");
      xmlprintf("<span class=\"inputunit\">\n");
      xmlprintf("genre:\n");
      showgenreform();
      xmlprintf("</span>\n");
      xmlprintf("</div>\n");
    }
  }
  xmlprintf("</form>\n");
  xmlprintf("</div>\n");
}


/* show the form for mail box */
static void showformformail(void){
  xmlprintf("<div id=\"estform\" class=\"estform\">\n");
  xmlprintf("<form action=\"%@\" method=\"get\" id=\"form_self\">\n", g_scriptname);
  shownaviform();
  if(p_navi == NM_ADVANCED){
    showadvancedform();
  } else {
    xmlprintf("<div class=\"form_basic\">\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("phrase:\n");
    xmlprintf("<input type=\"text\" name=\"phrase\" value=\"%@\""
              " size=\"48\" id=\"phrase\" class=\"text\" tabindex=\"%d\" accesskey=\"0\" />\n",
              p_phrase, ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("max:\n");
    showperpageform("changemax();");
    xmlprintf("</span>\n");
    if(g_clipview >= 0){
      xmlprintf("<span class=\"inputunit\">\n");
      xmlprintf("clip:\n");
      showclipform("changeclip();");
      xmlprintf("</span>\n");
    }
    xmlprintf("</div>\n");
    xmlprintf("<div class=\"form_extension\">\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("target:\n");
    xmlprintf("<input type=\"button\" value=\"text\""
              " onclick=\"changetarget('');\" onkeypress=\"changetarget('');\""
              " class=\"button mbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_attr[0] == '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"subject\""
              " onclick=\"changetarget('_subj_');\" onkeypress=\"changetarget('_subj_');\""
              " class=\"button mbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_attr, "_subj_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"from\""
              " onclick=\"changetarget('_from_');\" onkeypress=\"changetarget('_from_');\""
              " class=\"button mbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_attr, "_from_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"to,cc\""
              " onclick=\"changetarget('_toc_');\" onkeypress=\"changetarget('_toc_');\""
              " class=\"button mbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_attr, "_toc_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputunit\">\n");
    xmlprintf("order:\n");
    xmlprintf("<input type=\"button\" value=\"score\""
              " onclick=\"changeorder('');\" onkeypress=\"chaneorder('');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              p_order[0] == '\0' ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"date\""
              " onclick=\"changeorder('_date_');\" onkeypress=\"chaneorder('_date_');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_order, "_date_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("<input type=\"button\" value=\"size\""
              " onclick=\"changeorder('_size_');\" onkeypress=\"chaneorder('_size_');\""
              " class=\"button nbutton%s\" tabindex=\"%d\" accesskey=\"1\" />\n",
              !strcmp(p_order, "_size_") ? " abutton" : "", ++g_tabidx);
    xmlprintf("</span>\n");
    xmlprintf("<span class=\"inputhidden\">\n");
    xmlprintf("<input type=\"hidden\" name=\"navi\" value=\"%d\" id=\"navi\" />\n", p_navi);
    xmlprintf("<input type=\"hidden\" name=\"attr\" value=\"%@\" id=\"attr\" />\n", p_attr);
    xmlprintf("<input type=\"hidden\" name=\"order\" value=\"%@\" id=\"order\" />\n", p_order);
    xmlprintf("</span>\n");
    xmlprintf("</div>\n");
    if(cblistnum(g_genrechecks) > 0){
      xmlprintf("<div class=\"form_genrecheck\">\n");
      xmlprintf("<span class=\"inputunit\">\n");
      xmlprintf("genre:\n");
      showgenreform();
      xmlprintf("</span>\n");
      xmlprintf("</div>\n");
    }
  }
  xmlprintf("</form>\n");
  xmlprintf("</div>\n");
}


/* show the top message */
static void showtop(void){
  const char *path;
  char *text;
  if(p_navi == NM_ADVANCED) return;
  path = p_navi == NM_HELP ? g_helpfile : g_topfile;
  if((text = cbreadfile(path, NULL)) != NULL){
    printf("%s", text);
    free(text);
  } else {
    xmlprintf("<p>The file \"<code>%@</code>\" is not found.</p>\n", path);
  }
}


/* perform query expansion */
static void expandquery(const char *word, CBLIST *result){
  CBLIST *words;
  const char *tmpdir;
  char oname[PATH_MAX], cmd[PATH_MAX], *ebuf;
  int i;
  cblistpush(result, word, -1);
  tmpdir = getenv("TMP");
  if(!tmpdir) tmpdir = getenv("TEMP");
  if(!tmpdir) tmpdir = ESTPATHSTR "tmp";
  sprintf(oname, "%s%c%s.%08d", tmpdir, ESTPATHCHR, g_scriptname, (int)getpid());
  sprintf(cmd, "%s > %s", g_qxpndcmd, oname);
  ebuf = cbsprintf("ESTWORD=%s", word);
  putenv(ebuf);
  system(cmd);
  free(ebuf);
  if((words = cbreadlines(oname)) != NULL){
    for(i = 0; i < cblistnum(words); i++){
      word = cblistval(words, i, NULL);
      if(word[0] != '\0') cblistpush(result, word, -1);
    }
    cblistclose(words);
  }
  unlink(oname);
}


/* compare two keywords by scores in descending order */
static int keysc_compare(const void *ap, const void *bp){
  return ((KEYSC *)bp)->pt - ((KEYSC *)ap)->pt;
}


/* show the result */
static void showresult(ESTDOC **docs, int dnum, CBMAP *hints, ESTCOND *cond, int hits, int miss,
                       KEYSC *scores, int scnum){
  CBMAP *cnames;
  CBLIST *words;
  CBDATUM *abuf;
  const char *myphrase, *kbuf;
  char cname[NUMBUFSIZ], *myattr;
  const int *ary;
  int i, ksiz, snum, start, end, cnum, clip, anum, pnum;
  myphrase = p_similar > 0 ? "" : p_phrase;
  if(g_attrlist){
    abuf = cbdatumopen(NULL, -1);
    for(i = 0; i < cblistnum(g_attrlist); i++){
      if(i > 0) cbdatumcat(abuf, "\t", 1);
      cbdatumcat(abuf, cblistval(g_attrlist, i, NULL), -1);
    }
    myattr = cbdatumtomalloc(abuf, NULL);
  } else {
    myattr = cbmemdup(p_attr, -1);
  }
  xmlprintf("<div id=\"estresult\" class=\"estresult\">\n");
  hits -= miss;
  g_hnum = hits;
  start = (p_pagenum - 1) * p_perpage;
  end = p_pagenum * p_perpage;
  if(end > dnum) end = dnum;
  xmlprintf("<div class=\"resinfo\">");
  xmlprintf("Results of <strong>%d</strong> - <strong>%d</strong>",
            start + (hits > 0 ? 1 : 0), end);
  xmlprintf(" of about <strong>%d</strong>", hits);
  if(est_cond_auxiliary_word(cond, "")) xmlprintf(" or more");
  if(p_phrase[0] != '\0') xmlprintf(" for <strong>%48@</strong>", p_phrase);
  if(g_etime > 0.0) xmlprintf(" (%.3f sec.)", g_etime / 1000.0);
  if(miss > p_perpage * p_pagenum) xmlprintf("*");
  xmlprintf("</div>\n");
  if(cbmaprnum(hints) > 2 || (p_phrase[0] != '\0' && myattr[0] != '\0')){
    xmlprintf("<div class=\"hints\">");
    cbmapiterinit(hints);
    i = 0;
    while((kbuf = cbmapiternext(hints, &ksiz)) != NULL){
      if(ksiz < 1) continue;
      if(i++ > 0) xmlprintf(", ");
      xmlprintf("<span class=\"hword\">%@ (%@%@)</span>", kbuf, cbmapget(hints, kbuf, ksiz, NULL),
                est_cond_auxiliary_word(cond, kbuf) ? "+" : "");
    }
    xmlprintf("</div>\n");
  }
  if(scores && scnum > 0){
    xmlprintf("<div class=\"relkeys\">Related terms: ");
    for(i = 0; i < scnum && i < g_relkeynum; i++){
      if(i > 0) xmlprintf(", ");
      xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
                "&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d&amp;prec=%d\""
                " class=\"rword\">%@</a>",
                g_scriptname, p_navi, scores[i].word, myattr, p_attrval, p_order,
                p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec, scores[i].word);
    }
    xmlprintf("</div>\n");
  }
  words = est_hints_to_words(hints);
  cnames = cbmapopenex(MINIBNUM);
  cnum = 0;
  for(i = 0; i < cblistnum(words); i++){
    sprintf(cname, "key%d", ++cnum);
    cbmapput(cnames, cblistval(words, i, NULL), -1, cname, -1, FALSE);
  }
  clip = 0;
  if(p_clip > 0){
    for(i = 0; i < start && i < dnum; i++){
      est_cond_shadows(cond, est_doc_id(docs[i]), &anum);
      hits -= anum / 2;
    }
  }
  for(snum = start; snum < end; snum++){
    ary = est_cond_shadows(cond, est_doc_id(docs[snum]), &anum);
    showdoc(docs[snum], words, cnames, g_candetail && p_detail > 0, ary, anum, &clip);
    hits -= anum / 2;
  }
  cbmapclose(cnames);
  cblistclose(words);
  if(dnum < 1) xmlprintf("<p class=\"note\">Your search did not match any documents.</p>\n");
  xmlprintf("<div class=\"paging\">\n");
  if(clip > 0)
    xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
              "&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d&amp;cinc=-1"
              "&amp;prec=%d&amp;pagenum=%d&amp;similar=%d\" class=\"navi\">"
              "Include %d Clipped</a>\n",
              g_scriptname, p_navi, myphrase, myattr, p_attrval, p_order,
              p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec, p_pagenum, p_similar, clip);
  if(p_pagenum > 1){
    xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
              "&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d"
              "&amp;prec=%d&amp;pagenum=%d&amp;similar=%d\" class=\"navi\">PREV</a>\n",
              g_scriptname, p_navi, myphrase, myattr, p_attrval, p_order,
              p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec, p_pagenum - 1, p_similar);
  } else {
    xmlprintf("<span class=\"void\">PREV</span>\n");
  }
  pnum = (hits - 1 - (hits - 1) % p_perpage + p_perpage) / p_perpage;
  if(hits > 0 && p_detail < 1){
    for(i = p_pagenum > NAVIPAGES ? p_pagenum - NAVIPAGES + 1 : 1;
        i == 1 || (i <= pnum && i < p_pagenum + NAVIPAGES); i++){
      if(i == p_pagenum){
        printf("<span class=\"pnow\">%d</span>\n", i);
      } else {
        xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
                  "&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d"
                  "&amp;prec=%d&amp;pagenum=%d&amp;similar=%d\" class=\"pnum\">%d</a>\n",
                  g_scriptname, p_navi, myphrase, myattr, p_attrval,
                  p_order, p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec, i, p_similar, i);
      }
    }
  }
  if(snum < dnum){
    xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
              "&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d&amp;prec=%d&amp;pagenum=%d"
              "&amp;similar=%d\" class=\"navi\">NEXT</a>\n",
              g_scriptname, p_navi, myphrase, myattr, p_attrval, p_order,
              p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec, p_pagenum + 1, p_similar);
  } else {
    xmlprintf("<span class=\"void\">NEXT</span>\n");
    if(est_cond_auxiliary_word(cond, ""))
      xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
                "&amp;perpage=%d&amp;prec=1&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d"
                "&amp;similar=%d\" class=\"navi\">Search More Precisely</a>\n",
                g_scriptname, p_navi, myphrase, myattr, p_attrval, p_order,
                p_perpage, p_clip, p_qxpnd, p_gmasks, p_similar);
  }
  xmlprintf("</div>\n");
  xmlprintf("</div>\n");
  free(myattr);
}


/* show a document */
static void showdoc(ESTDOC *doc, const CBLIST *words, CBMAP *cnames, int detail,
                    const int *shadows, int snum, int *clipp){
  ESTDOC *tdoc;
  CBMAP *kwords;
  CBLIST *names, *lines;
  const char *uri, *title, *score, *val, *name, *line, *cname;
  char *turi, *tsv, *pv, *str, numbuf[NUMBUFSIZ];
  int i, id, wwidth, hwidth, awidth;
  id = est_doc_id(doc);
  if(g_showlreal){
    if(!(uri = est_doc_attr(doc, DATTRLREAL)) && !(uri = est_doc_attr(doc, ESTDATTRURI)))
      uri = ".";
  } else {
    if(!(uri = est_doc_attr(doc, ESTDATTRURI))) uri = ".";
  }
  turi = makeshownuri(uri);
  if(!(title = est_doc_attr(doc, ESTDATTRTITLE))) title = "";
  if(title[0] == '\0' && !(title = est_doc_attr(doc, DATTRLFILE))) title = "";
  if(title[0] == '\0' && ((pv = strrchr(uri, '/')) != NULL)) title = pv + 1;
  if(title[0] == '\0') title = "(no title)";
  if(!(score = est_doc_attr(doc, DATTRSCORE))) score = "";
  xmlprintf("<dl class=\"doc\" id=\"doc_%d\">\n", id);
  xmlprintf("<dt>");
  xmlprintf("<a href=\"%@\" class=\"doc_title\">", turi);
  sprintf(numbuf, "%%%d@", detail ? 9999 : g_attrwidth);
  xmlprintf(numbuf, title);
  xmlprintf("</a>");
  if(score[0] != '\0' && p_detail < 1) xmlprintf(" <span class=\"doc_score\">%@</span>", score);
  xmlprintf("</dt>\n");
  if(detail){
    names = est_doc_attr_names(doc);
    for(i = 0; i < cblistnum(names); i++){
      name = cblistval(names, i, NULL);
      if(name[0] != '_' && strcmp(name, ESTDATTRURI) && strcmp(name, ESTDATTRTITLE) &&
         (val = est_doc_attr(doc, name)) != NULL && val[0] != '\0'){
        xmlprintf("<dd class=\"doc_attr\">");
        xmlprintf("%@: <span class=\"doc_attr_val doc_val_%$\">%@</span>", name, name, val);
        xmlprintf("</dd>\n");
      }
    }
    cblistclose(names);
    if(g_smlrvnum > 0){
      xmlprintf("<dd class=\"doc_attr\">");
      xmlprintf("#keywords: <span class=\"doc_val doc_val_kwords\">");
      kwords = est_db_get_keywords(g_db, id);
      if(!kwords) kwords = est_db_etch_doc(g_db, doc, g_smlrvnum);
      cbmapiterinit(kwords);
      for(i = 0; (name = cbmapiternext(kwords, NULL)) != NULL; i++){
        if(i > 0) xmlprintf(", ");
        xmlprintf("<span class=\"kwelem\">%@ (%@)</span>",
                  name, cbmapget(kwords, name, -1, NULL));
      }
      cbmapclose(kwords);
      xmlprintf("</span>");
      xmlprintf("</dd>\n");
    }
  } else {
    for(i = 0; i < cblistnum(g_extattrs); i++){
      str = cbmemdup(cblistval(g_extattrs, i, NULL), -1);
      if((pv = strchr(str, '|')) != NULL){
        *pv = '\0';
        pv++;
        if((val = est_doc_attr(doc, str)) != NULL && val[0] != '\0'){
          xmlprintf("<dd class=\"doc_attr\">");
          xmlprintf("%@: <span class=\"doc_val doc_val_%$\">", pv, str);
          sprintf(numbuf, "%%%d@", g_attrwidth);
          xmlprintf(numbuf, val);
          xmlprintf("</span>");
          xmlprintf("</dd>\n");
        }
      }
      free(str);
    }
  }
  xmlprintf("<dd class=\"doc_text\">");
  if(detail){
    wwidth = INT_MAX;
    hwidth = INT_MAX;
    awidth = 0;
  } else if(shadows){
    wwidth = g_snipwwidth;
    hwidth = g_sniphwidth;
    awidth = g_snipawidth;
  } else {
    wwidth = g_snipwwidth * 0.7;
    hwidth = g_sniphwidth * 0.8;
    awidth = g_snipawidth * 0.6;
  }
  tsv = est_doc_make_snippet(doc, words, wwidth, hwidth, awidth);
  lines = cbsplit(tsv, -1, "\n");
  for(i = 0; i < cblistnum(lines); i++){
    line = cblistval(lines, i, NULL);
    if(line[0] == '\0'){
      if(i < cblistnum(lines) - 1) xmlprintf(" <code class=\"delim\">...</code> ");
    } else if((pv = strchr(line, '\t')) != NULL){
      str = cbmemdup(line, pv - line);
      if(!(cname = cbmapget(cnames, pv + 1, -1, NULL))) cname = "key0";
      xmlprintf("<strong class=\"key %@\">%@</strong>", cname, str);
      free(str);
    } else {
      xmlprintf("%@", line);
    }
  }
  cblistclose(lines);
  free(tsv);
  xmlprintf("</dd>\n");
  xmlprintf("<dd class=\"doc_navi\">\n");
  xmlprintf("<span class=\"doc_link\">");
  sprintf(numbuf, "%%%d@", detail ? 9999 : g_attrwidth);
  xmlprintf(numbuf, turi);
  xmlprintf("</span>\n");
  if(*g_dispproxy != '\0'){
    if(!strcmp(g_dispproxy, "[URI]")){
      xmlprintf("- <a href=\"%s", turi);
    } else {
      xmlprintf("- <a href=\"%s?url=%?", g_dispproxy, turi);
    }
    for(i = 0; i < cblistnum(words); i++){
      xmlprintf("&amp;word%d=%?", i + 1, cblistval(words, i, NULL));
    }
    xmlprintf("&amp;once=1\" class=\"display\">[display]</a>\n");
  }
  if(g_candetail)
    xmlprintf("- <a href=\"%@?navi=%d&amp;phrase=%?&amp;detail=%d&amp;perpage=%d&amp;clip=%d"
              "&amp;qxpnd=%d&amp;gmasks=%d&amp;prec=%d\" class=\"detail\">[detail]</a>\n",
              g_scriptname, p_navi, p_similar > 0 ? "" : p_phrase, id,
              p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec);
  if(g_smlrvnum > 0)
    xmlprintf("- <a href=\"%@?navi=%d&amp;similar=%d&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d"
              "&amp;gmasks=%d&amp;prec=%d\" class=\"similar\">[similar]</a>\n",
              g_scriptname, p_navi, id, p_perpage, p_clip, p_qxpnd, p_gmasks, p_prec);
  if(g_candir){
    str = cbmemdup(turi, -1);
    if((str[0] == '\\' && str[1] == '\\') ||
       (((str[0] >= 'A' && str[1] <= 'Z') || (str[0] >= 'a' && str[1] <= 'z')) &&
        str[1] == ':' && str[2] == '\\')){
      if((pv = strrchr(str, '\\')) != NULL) pv[1] = '\0';
    } else if((pv = strrchr(str, '/')) != NULL){
      pv[1] = '\0';
    }
    xmlprintf("- <a href=\"%@\" class=\"dir\">[dir]</a>\n", str);
    free(str);
  }
  xmlprintf("</dd>\n");
  xmlprintf("</dl>\n");
  if(!detail && shadows && snum > 0){
    for(i = 0; i < snum; i += 2){
      if(p_cinc >= 0 && p_cinc != id && i >= g_clipview * 2){
        xmlprintf("<div class=\"doc_clip\">\n");
        xmlprintf("<p>%d more documents clipped ... ", (snum - i) / 2);
        xmlprintf("<a href=\"%@?navi=%d&amp;phrase=%?&amp;attr=%?&amp;attrval=%?&amp;order=%?"
                  "&amp;perpage=%d&amp;clip=%d&amp;qxpnd=%d&amp;gmasks=%d&amp;cinc=%d"
                  "&amp;prec=%d&amp;pagenum=%d&amp;similar=%d#doc_%d\" class=\"include\">"
                  "[include]</a>",
                  g_scriptname, p_navi, p_similar > 0 ? "" : p_phrase, p_attr, p_attrval, p_order,
                  p_perpage, p_clip, p_qxpnd, p_gmasks, id, p_prec, p_pagenum, p_similar, id);
        xmlprintf("</p>\n");
        xmlprintf("</div>\n");
        *clipp += (snum - i) / 2;
        break;
      }
      if(!(tdoc = est_db_get_doc(g_db, shadows[i], 0))) continue;
      if(g_showscore){
        sprintf(numbuf, "%1.3f", shadows[i+1] >= 9999 ? 1.0 : shadows[i+1] / 10000.0);
        est_doc_add_attr(tdoc, DATTRSCORE, numbuf);
      }
      xmlprintf("<div class=\"doc_clip\">\n");
      showdoc(tdoc, words, cnames, FALSE, NULL, 0, NULL);
      xmlprintf("</div>\n");
      est_doc_delete(tdoc);
    }
  }
  free(turi);
}


/* make a URI to be shown */
static char *makeshownuri(const char *uri){
  char *turi, *bef, *aft, *pv, *nuri;
  int i;
  turi = cbmemdup(uri, -1);
  for(i = 0; i < cblistnum(g_replexprs); i++){
    bef = cbmemdup(cblistval(g_replexprs, i, NULL), -1);
    if((pv = strstr(bef, "{{!}}")) != NULL){
      *pv = '\0';
      aft = pv + 5;
    } else {
      aft = "";
    }
    nuri = est_regex_replace(turi, bef, aft);
    free(turi);
    turi = nuri;
    free(bef);
  }
  return turi;
}


/* show the top */
static void showinfo(void){
  xmlprintf("<div id=\"estinfo\" class=\"estinfo\">");
  xmlprintf("Powered by <a href=\"%@\">Hyper Estraier</a> %@, with %d documents and %d words.",
            _EST_PROJURL, est_version, est_db_doc_num(g_db), est_db_word_num(g_db));
  xmlprintf("</div>\n");
}


/* output the log message */
static void outputlog(void){
  FILE *ofp;
  CBDATUM *condbuf;
  const char *rp, *pv;
  char *name, *value;
  if(g_logfile[0] == '\0' || p_pagenum > 1) return;
  condbuf = cbdatumopen(NULL, -1);
  if(*p_phrase != '\0') cbdatumcat(condbuf, p_phrase, -1);
  if(*p_attr != '\0'){
    if(cbdatumsize(condbuf) > 0) cbdatumcat(condbuf, " ", 1);
    cbdatumcat(condbuf, "{{attr:", -1);
    cbdatumcat(condbuf, p_attr, -1);
    cbdatumcat(condbuf, "}}", -1);
  }
  if(*p_order != '\0'){
    if(cbdatumsize(condbuf) > 0) cbdatumcat(condbuf, " ", 1);
    cbdatumcat(condbuf, "{{order:", -1);
    cbdatumcat(condbuf, p_order, -1);
    cbdatumcat(condbuf, "}}", -1);
  }
  if(cbdatumsize(condbuf) < 1 || !(ofp = fopen(g_logfile, "ab"))){
    cbdatumclose(condbuf);
    return;
  }
  rp = g_logformat;
  while(*rp != '\0'){
    switch(*rp){
    case '\\':
      if(rp[1] != '\0') rp++;
      switch(*rp){
      case 't':
        fputc('\t', ofp);
        break;
      case 'n':
        fputc('\n', ofp);
        break;
      default:
        fputc(*rp, ofp);
        break;
      }
      break;
    case '{':
      if(cbstrfwmatch(rp, "{cond}")){
        pv = cbdatumptr(condbuf);
        while(*pv != '\0'){
          if(*pv > '\0' && *pv < ' '){
            fputc(' ', ofp);
          } else {
            fputc(*pv, ofp);
          }
          pv++;
        }
        rp += 5;
      } else if(cbstrfwmatch(rp, "{time}")){
        value = cbdatestrwww(-1, 0);
        fprintf(ofp, "%s", value);
        free(value);
        rp += 5;
      } else if(cbstrfwmatch(rp, "{hnum}")){
        fprintf(ofp, "%d", g_hnum);
        rp += 5;
      } else if((pv = strchr(rp, '}')) != NULL){
        rp++;
        name = cbmemdup(rp, pv - rp);
        value = getenv(name);
        if(value) fprintf(ofp, "%s", value);
        free(name);
        rp = pv;
      } else {
        fputc(*rp, ofp);
      }
      break;
    default:
      fputc(*rp, ofp);
      break;
    }
    rp++;
  }
  fclose(ofp);
  cbdatumclose(condbuf);
}



/* END OF FILE */
