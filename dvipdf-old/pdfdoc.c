#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pdfobj.h"
#include "error.h"
#include "mem.h"
#include "config.h"
#include "pdfdoc.h"
#include "numbers.h"

static pdf_obj *catalog = NULL;
static pdf_obj *docinfo = NULL;
static pdf_obj *page_tree = NULL, *page_tree_label = NULL, *pages_kids = NULL;

#define MAX_OUTLINE_DEPTH 16
int outline_depth=0;
static struct 
{
  int kid_count;
  pdf_obj *entry;
} outline[MAX_OUTLINE_DEPTH];


static pdf_obj *current_page_resources = NULL;
static pdf_obj *page_count_obj = NULL, *this_page_contents = NULL;
static pdf_obj *page_bop, *page_eop;
static pdf_obj *this_page_beads = NULL;
static pdf_obj *this_page_annots = NULL;
static pdf_obj *this_page_xobjects = NULL;
static pdf_obj *tmp1, *tmp2, *tmp3;

#define MAX_PAGES 4096
static int page_count = 0;
static struct {
  pdf_obj *page_dict;
  pdf_obj *page_ref;
} pages[MAX_PAGES];

static start_page_tree (void);
static create_catalog (void);
static start_current_page_resources (void);
static finish_page_tree(void);

static void start_name_tree(void);
static void finish_name_tree(void);

static void start_dests_tree(void);
static void finish_dests_tree(void);

static void start_articles(void);

static verbose = 0;
static debug = 0;

void pdf_doc_set_verbose(void)
{
  verbose = 1;
}

void pdf_doc_set_debug(void)
{
  debug = 1;
}

static start_page_tree (void)
{
  if (debug) {
    fprintf (stderr, "(start_page_tree)");
  }
  /* Create empty page tree */
  page_tree = pdf_new_dict();
  page_tree_label = pdf_ref_obj (page_tree);
  /* page_tree goes away fairly soon.  Use page_tree_label
     to refer to this object.  page_tree_label is kept
     until document is closed */
  /* Link it into the catalog */
  /* Point /Pages attribute to indirect reference since we
     haven't built the tree yet */
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Pages"),
		page_tree_label);
  pdf_release_obj (tmp1);
  page_bop = pdf_new_stream();
  page_eop = pdf_new_stream();
}

void pdf_doc_bop (char *string, unsigned length)
{
  if (length > 0)
    pdf_add_stream (page_bop, string, length);
}

void pdf_doc_eop (char *string, unsigned length)
{
  if (length > 0)
    pdf_add_stream (page_eop, string, length);
}

static start_outline_tree (void)
{
  if (debug) {
    fprintf (stderr, "(start_outline_tree)");
  }
  /* Create empty outline tree */
  outline[outline_depth].entry = pdf_new_dict();
  outline[outline_depth].kid_count = 0;
  /* Link it into the catalog */
  /* Point /Outline attribute to indirect reference */
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Outlines"),
		tmp2 = pdf_ref_obj(outline[outline_depth].entry));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
}

static pdf_obj *names_dict;

static void start_name_tree (void)
{
  if (debug) {
    fprintf (stderr, "(start_name_tree)");
  }
  names_dict = pdf_new_dict ();
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Names"),
		tmp2 = pdf_ref_obj (names_dict));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
}

static void finish_name_tree (void)
{
  pdf_release_obj (names_dict);
  return;
}



static char *asn_date (void)
{
  static char date_string[22];
  time_t current_time;
  struct tm *bd_time;
  if (debug) {
    fprintf (stderr, "(asn_date)");
  }
  time(&current_time);
  bd_time = localtime(&current_time);
  sprintf (date_string, "D:%04d%02d%02d%02d%02d%02d%0+3d'%02d'",
	   bd_time -> tm_year+1900, bd_time -> tm_mon+1, bd_time -> tm_mday,
	   bd_time -> tm_hour, bd_time -> tm_min, bd_time -> tm_sec,
	   -timezone/3600, timezone%3600);
  return date_string;
}

#define BANNER "dvipdf %s, Copyright 1998, by Mark A. Wicks"
static create_docinfo (void)
{
  pdf_obj *tmp1, *tmp2;
  char *time_string, *banner;
  /* Create an empty Info entry and put
     into root object */
  if (debug) {
    fprintf (stderr, "(create_docinfo)");
  }
  docinfo = pdf_new_dict ();
  banner = NEW (strlen(BANNER)+20,char);
  sprintf(banner, BANNER, VERSION);
  pdf_add_dict (docinfo, 
		tmp1 = pdf_new_name ("Producer"),
		tmp2 = pdf_new_string (banner, strlen (banner)));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  release (banner);
  
  time_string = asn_date();
  pdf_add_dict (docinfo, 
		tmp1 = pdf_new_name ("CreationDate"),
		tmp2 = pdf_new_string (time_string, strlen (time_string)));
  pdf_set_info (docinfo);
}

void pdf_doc_merge_with_docinfo (pdf_obj *dictionary)
{
  pdf_merge_dict (docinfo, dictionary);
}

void pdf_doc_merge_with_catalog (pdf_obj *dictionary)
{
  pdf_merge_dict (catalog, dictionary);
}

static create_catalog (void)
{
  if (debug) {
    fprintf (stderr, "(create_catalog)");
  }
  catalog = pdf_new_dict ();
  pdf_set_root (catalog);
  /* Create /Type attribute */
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Type"),
		tmp2 = pdf_new_name("Catalog"));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
 /* Create only those parts of the page tree required for the catalog.
    That way, the rest of the page tree can be finished at any time */
  start_page_tree(); 
  /* Likewise for outline tree */
  start_outline_tree ();
  start_name_tree();
  start_dests_tree();
  finish_name_tree();
  start_articles();
}

static start_current_page_resources (void)
{
  /* work on resources to put in Pages */
  if (debug) {
    fprintf (stderr, "(start_current_page_resources)");
  }
  current_page_resources = pdf_new_dict ();
  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, tmp2 = pdf_new_name ("PDF"));
  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_name ("Text"));
  pdf_release_obj (tmp2);
  pdf_add_dict (current_page_resources,
		tmp2 = pdf_new_name ("ProcSet"),
		tmp1);
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  this_page_xobjects = pdf_new_dict ();
  pdf_add_dict (current_page_resources,
		tmp1 = pdf_new_name ("XObject"),
		tmp2 = pdf_ref_obj (this_page_xobjects));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
}

void pdf_doc_add_to_page_xobjects (const char *name, pdf_obj
				   *resource)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_xojects)");
  }
  pdf_add_dict (this_page_xobjects,
		tmp1 = pdf_new_name (name), 
		resource);
  pdf_release_obj(tmp1); 
}


void pdf_doc_add_to_page_resources (const char *name, pdf_obj *resource)
{
  pdf_obj *tmp1;
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_resources)");
  }
  pdf_add_dict (current_page_resources,
		tmp1 = pdf_new_name (name), 
		resource);
  pdf_release_obj(tmp1); 
}

void pdf_doc_add_to_page_annots (pdf_obj *annot)
{
  pdf_obj *tmp1;
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_annots)");
  }
  pdf_add_array (this_page_annots,
		 annot);
}


static finish_page_tree(void)
{
  /* Back to work on that page tree */
  if (debug) {
    fprintf (stderr, "(finish_page_tree)");
  }
  pdf_add_dict (page_tree,
		tmp1 = pdf_new_name ("Type"),
		tmp2 = pdf_new_name ("Pages"));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  page_count_obj = pdf_new_number (0);
  pdf_add_dict (page_tree,
		tmp1 = pdf_new_name ("Count"),
		tmp2 = pdf_ref_obj(page_count_obj));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  pages_kids = pdf_new_array ();
  pdf_add_dict (page_tree,
		tmp1 = pdf_new_name ("Kids"),
		tmp2 = pdf_ref_obj (pages_kids));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  /* Page_Tree is done.  */
  pdf_release_obj (page_tree);
}

void pdf_doc_change_outline_depth(int new_depth)
{
  int i;
  if (debug) {
    fprintf (stderr, "(change_outline_depth)");
  }
  if (outline_depth >= MAX_OUTLINE_DEPTH -1)
    ERROR ("Outline is too deep.");
  if (new_depth == outline_depth)
    /* Nothing to do */
    return;
  if (new_depth > outline_depth+1)
    ERROR ("Can't increase outline depth by more than one at a time\n");
  if (outline[outline_depth].entry == NULL)
    ERROR ("change_outline_depth: Fix me, I'm broke. This shouldn't happen!");
  /* Terminate all entries above this depth */
  for (i=outline_depth-1; i>=new_depth; i--) {
    tmp1 = pdf_ref_obj (outline[i+1].entry);
    pdf_add_dict (outline[i].entry,
		  tmp2 = pdf_new_name ("Last"),
		  tmp1);
    pdf_release_obj (tmp1); pdf_release_obj (tmp2);
    outline[i].kid_count += outline[i+1].kid_count;
    if (i > 0) 
      tmp1 = pdf_new_number (-outline[i].kid_count);
    else
      tmp1 = pdf_new_number (outline[i].kid_count);

    pdf_add_dict (outline[i].entry,
		  tmp2 = pdf_new_name ("Count"),
		  tmp1);
    pdf_release_obj (tmp1); pdf_release_obj (tmp2); 
  }
  /* Flush out all entries above this depth */
  for (i=new_depth+1; i<=outline_depth; i++) {
    pdf_release_obj (outline[i].entry);
    outline[i].entry = NULL;
    outline[i].kid_count = 0;
  }
  outline_depth = new_depth;
}

static void finish_outline(void)
{
  pdf_doc_change_outline_depth (0);
  pdf_release_obj (outline[0].entry);
  outline[0].entry = NULL;
}


void pdf_doc_add_outline (pdf_obj *dict)
{
  pdf_obj *new_entry;
  if (outline_depth < 1)
    ERROR ("Can't add to outline at depth < 1");
  new_entry = pdf_new_dict ();
  /* Tell it where its parent is */
  pdf_add_dict (new_entry,
		tmp1 = pdf_new_name ("Parent"),
		tmp2 = pdf_ref_obj (outline[outline_depth-1].entry));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  /* Give mom and dad the good news */
  outline[outline_depth-1].kid_count += 1;

  /* Is this the first entry at this depth? */
  if (outline[outline_depth].entry == NULL) {
    /* Is so, tell the parent we are first born */
    pdf_add_dict (outline[outline_depth-1].entry,
		  tmp1 = pdf_new_name ("First"),
		  tmp2 = pdf_ref_obj (new_entry));
    pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  }
  else {
    /* Point our elder sister toward us */
    pdf_add_dict (outline[outline_depth].entry,
		  tmp1 = pdf_new_name ("Next"),
		  tmp2 = pdf_ref_obj (new_entry));
    pdf_release_obj (tmp1); pdf_release_obj (tmp2);
    /* Point us back to sister */
    pdf_add_dict (new_entry,
		  tmp1 = pdf_new_name ("Previous"),
		  tmp2 = pdf_ref_obj (outline[outline_depth].entry));
    pdf_release_obj (tmp1); pdf_release_obj (tmp2);
    /* Tell parents about grandchildren before killing sis */
    outline[outline_depth-1].kid_count += outline[outline_depth].kid_count;
    /* Bye-Bye sis */
    pdf_release_obj (outline[outline_depth].entry);
  }
  pdf_merge_dict (new_entry, dict);
  outline[outline_depth].entry = new_entry;
  /* Just born, so don't have any kids */
  outline[outline_depth].kid_count = 0;
}


#define MAX_DESTS 2048
struct dests 
{
  char *name;
  unsigned length;
  pdf_obj *array;
};
typedef struct dests dest_entry;
static dest_entry dests[MAX_DESTS];

static number_dests = 0;


static pdf_obj *dests_dict;

static void start_dests_tree (void)
{
  if (debug) {
    fprintf (stderr, "(start_dests_tree)");
  }
  dests_dict = pdf_new_dict();
  pdf_add_dict (names_dict,
		tmp1 = pdf_new_name ("Dests"),
		tmp2 = pdf_ref_obj (dests_dict));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
}


#define MIN(a,b) ((a)<(b)? (a): (b))

static int cmp_dest (const void *d1, const void *d2)
{
  unsigned length;
  int tmp;
  length = MIN (((dest_entry *) d1) -> length, ((dest_entry *) d2) ->
		length);
  if (tmp = strncmp (((dest_entry *) d1) -> name, ((dest_entry *) d2) -> name, length))
    return tmp;
  if (((dest_entry *) d1) -> length == ((dest_entry *) d2) -> length)
    return 0;
  return (((dest_entry *) d1) -> length < ((dest_entry *) d2) -> length ? -1 : 1 );
}

static void finish_dests_tree (void)
{
  pdf_obj *kid, *node, *name_array;
  int i;
  name_array = pdf_new_array ();
  qsort(dests, number_dests, sizeof(dests[0]), cmp_dest);
  for (i=0; i<number_dests; i++) {
    pdf_add_array (name_array, tmp1 = pdf_new_string (dests[i].name,
						      dests[i].length));
    pdf_add_array (name_array, tmp2 = pdf_link_obj (dests[i].array));
    pdf_release_obj (tmp1);
    pdf_release_obj (tmp2);
  }
  kid = pdf_new_dict ();
  pdf_add_dict (kid,
		tmp1 = pdf_new_name ("Names"),
		name_array);
  pdf_release_obj (tmp1);
  pdf_release_obj (name_array);
  tmp1 = pdf_new_array();
  pdf_add_array (tmp1, tmp2 = pdf_new_string (dests[0].name,
					      dests[0].length));
  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_string (dests[number_dests-1].name,
					      dests[number_dests-1].length));
  pdf_release_obj (tmp2);
  pdf_add_dict (kid,
		tmp2 = pdf_new_name ("Limits"),
		tmp1);
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);

  tmp2 = pdf_new_array();
  tmp3 = pdf_ref_obj (kid);
  pdf_add_array (tmp2, tmp3);
  pdf_release_obj (tmp3);
  pdf_add_dict (dests_dict,
		tmp1 = pdf_new_name ("Kids"),
		tmp2);
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  pdf_release_obj (kid);
  pdf_release_obj (dests_dict);
}


void pdf_doc_add_dest (char *name, unsigned length, pdf_obj *array )
{
  if (number_dests >= MAX_DESTS) {
    ERROR ("pdf_doc_add_dest:  Too many named destinations\n");
  }
  dests[number_dests].name = NEW (length, char);
  strncpy (dests[number_dests].name, name, length);
  dests[number_dests].length = length;
  dests[number_dests].array = pdf_ref_obj (array);
  number_dests++;
}

#define MAX_ARTICLES 32
struct articles
{
  char *name;
  pdf_obj *info;
  pdf_obj *first;
  pdf_obj *last;
  pdf_obj *this;
};

typedef struct articles article_entry;
static article_entry articles[MAX_ARTICLES];
static number_articles = 0;

static pdf_obj *articles_array;
static void start_articles (void)
{
  articles_array = pdf_new_array();
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Threads"),
		tmp2 = pdf_ref_obj (articles_array));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
}

pdf_obj *pdf_doc_add_article (char *name, pdf_obj *info)
{
  pdf_obj *result;
  if (number_articles >= MAX_ARTICLES) {
    ERROR ("pdf_doc_add_article:  Too many articles\n");
  }
  articles[number_articles].name = NEW (strlen(name)+1, char);
  strcpy (articles[number_articles].name, name);
  articles[number_articles].info = pdf_link_obj (info);
  articles[number_articles].first = NULL;
  articles[number_articles].last = NULL;
  /* Start dictionary for this article even though we can't finish it
     until we get the first bead */
  result = pdf_new_dict();
  articles[number_articles].this = pdf_link_obj(result);
  number_articles++;
  return result;
}

void pdf_doc_add_bead (char *article_name, pdf_obj *partial_dict)
{
  /* partial_dict should have P (Page) and R (Rect) already filled in */
  /* See if the specified article exists */
  int i;
  for (i=0; i<number_articles; i++) {
    if (!strcmp (articles[i].name, article_name))
      break;
  }
  if (i == number_articles) {
    fprintf (stderr, "Bead specified thread that doesn't exist\n");
    return;
  }
  /* Is this the first bead? */
  if (articles[i].last == NULL) {
    articles[i].first = pdf_link_obj (partial_dict);
    /* Add pointer to its first object */ 
    pdf_add_dict (articles[i].this,
		  tmp2 = pdf_new_name ("F"),
		  tmp3 = pdf_ref_obj (articles[i].first));
    pdf_release_obj (tmp2);
    pdf_release_obj (tmp3);
    /* Next add pointer to its Info dictionary */
    pdf_add_dict (articles[i].this,
		  tmp2 = pdf_new_name ("I"),
		  articles[i].info);
    pdf_release_obj (tmp2);
    /* Point first bead to parent article */
    pdf_add_dict (partial_dict,
		  tmp1 = pdf_new_name ("T"),
		  tmp2 = pdf_ref_obj (articles[i].this));
    pdf_release_obj (tmp2);
    /* Ship it out and forget it */
    pdf_add_array (articles_array, tmp1 = pdf_ref_obj (articles[i].this));
    pdf_release_obj (tmp1);
    pdf_release_obj (articles[i].this);
    articles[i].this = NULL;
    pdf_release_obj (articles[i].info);
    articles[i].info = NULL;
  } else {
    /* Link it in... */
    /* Point last object to this one */
    pdf_add_dict (articles[i].last,
		  tmp1 = pdf_new_name ("N"),
		  tmp2 = pdf_ref_obj (partial_dict));
    pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
    /* Point this one to last */
    pdf_add_dict (partial_dict,
		  tmp1 = pdf_new_name ("V"),
		  tmp2 = pdf_ref_obj (articles[i].last));
    pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
    pdf_release_obj (articles[i].last);
  }
  articles[i].last = pdf_link_obj (partial_dict);
  pdf_add_array (this_page_beads,
		 tmp1 = pdf_ref_obj (partial_dict));
  pdf_release_obj (tmp1);
}

void finish_articles(void)
{
  int i;
  pdf_release_obj (articles_array);
  for (i=0; i<number_articles; i++) {
    if (articles[i].last == NULL) {
      fprintf (stderr, "Article started, but no beads\n");
      break;
    }
    /* Close the loop */
    pdf_add_dict (articles[i].last,
		  tmp1 = pdf_new_name ("N"),
		  tmp2 = pdf_ref_obj (articles[i].first));
    pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
    pdf_add_dict (articles[i].first,
		  tmp1 = pdf_new_name ("V"),
		  tmp2 = pdf_ref_obj (articles[i].last));
    pdf_release_obj (tmp1);  pdf_release_obj (tmp2);
    pdf_release_obj (articles[i].first);
    pdf_release_obj (articles[i].last);
  }
}


static void finish_last_page ()
{
  if (debug) {
    fprintf (stderr, "(finish_last_page)");
  }
  if (this_page_contents != NULL) {
    pdf_release_obj (this_page_contents);
    this_page_contents = NULL;
  }
  if (current_page_resources != NULL) {
    pdf_release_obj (current_page_resources);
    current_page_resources = NULL;
  }
  if (this_page_annots != NULL) {
    pdf_release_obj (this_page_annots);
    this_page_annots = NULL;
  }
  if (this_page_beads != NULL) {
    pdf_release_obj (this_page_beads);
    this_page_beads = NULL;
  }
  if (this_page_xobjects != NULL) {
    pdf_release_obj (this_page_xobjects);
    this_page_xobjects = NULL;
  }
}

static int highest_page_ref = 0;
pdf_obj *pdf_doc_ref_page (unsigned page_no)
{
  if (page_no >= MAX_PAGES)
    ERROR ("Reference to page greater than MAX_PAGES");
  /* Has this page been referenced yet? */ 
  if (pages[page_no-1].page_ref == NULL) {
    /* If not, create it */
    pages[page_no-1].page_dict = pdf_new_dict ();
    /* and reference it */
    pages[page_no-1].page_ref = pdf_ref_obj (pages[page_no-1].page_dict);
  }
  if (page_no > highest_page_ref)
    highest_page_ref = page_no;
  return pdf_link_obj (pages[page_no-1].page_ref);
}

pdf_obj *pdf_doc_this_page (void)
{
  pdf_obj *result;
  if (page_count <= 0) {
    ERROR ("Reference to current page, but no pages have been started yet");
  }
  return pdf_doc_ref_page(page_count);
}

void pdf_doc_new_page (double page_width, double page_height)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_new_page)");
  }
  if (page_count >= MAX_PAGES) 
    ERROR ("Too many pages in pdf_doc_new_page\n");
  
  if (this_page_contents != NULL) {
    finish_last_page();
  }
  /* Was this page already instantiated by a forward reference to it? */
  if (pages[page_count].page_dict == NULL) {
    /* If not, create it. */
    pages[page_count].page_dict = pdf_new_dict ();
    /* and reference it */
    pages[page_count].page_ref = pdf_ref_obj(pages[page_count].page_dict);
  }
  pdf_add_array (pages_kids,
		 pages[page_count].page_ref);

  pdf_add_dict (pages[page_count].page_dict,
		tmp1 = pdf_new_name ("Type"),
		tmp2 = pdf_new_name ("Page"));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);

  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (ROUND(page_width,1.0))); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (ROUND(page_height,1.0))); pdf_release_obj (tmp2);
  pdf_add_dict (pages[page_count].page_dict,
		tmp2 = pdf_new_name ("MediaBox"),
		tmp1);
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  pdf_add_dict (pages[page_count].page_dict,
		tmp1 = pdf_new_name ("Parent"),
		page_tree_label);
  pdf_release_obj (tmp1);

  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, tmp2 = pdf_ref_obj (page_bop));
  pdf_release_obj (tmp2);
  this_page_contents = pdf_new_stream();
  pdf_add_array (tmp1, tmp2 = pdf_ref_obj (this_page_contents));
  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_ref_obj (page_eop));
  pdf_release_obj (tmp2);
  pdf_add_dict (pages[page_count].page_dict,
		tmp2 = pdf_new_name ("Contents"),
		tmp1);
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);

  this_page_annots = pdf_new_array ();
  pdf_add_dict (pages[page_count].page_dict,
		tmp1 = pdf_new_name ("Annots"),
		tmp2 = pdf_ref_obj (this_page_annots));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  
  start_current_page_resources();
  pdf_add_dict (pages[page_count].page_dict,
		tmp1 = pdf_new_name ("Resources"),
		tmp2 = pdf_ref_obj (current_page_resources));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  this_page_beads = pdf_new_array();
  pdf_add_dict (pages[page_count].page_dict,
		tmp1 = pdf_new_name ("B"),
		tmp2 = pdf_ref_obj (this_page_beads));
  pdf_release_obj (tmp1); pdf_release_obj (tmp2);
  /* Flush this page */
  pdf_release_obj (pages[page_count].page_dict);
  pages[page_count].page_dict = NULL;
  /* Contents are still available as this_page_contents until next
     page is started */
  /* Even though the page is gone, a Reference to this page is kept until program ends */
  page_count += 1;
}

void pdf_doc_add_to_page (char *buffer, unsigned length)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page)");
  }
  pdf_add_stream (this_page_contents, buffer, length);
}

void pdf_doc_init (char *filename) 
{
  if (debug) fprintf (stderr, "pdf_doc_init:\n");
  pdf_out_init (filename);
  create_docinfo ();
  create_catalog ();
  finish_page_tree();	/* start_page_tree was called by create_catalog */
}

void pdf_doc_finish ()
{
  int i;
  if (debug) fprintf (stderr, "pdf_doc_finish:\n");
  if (this_page_contents != NULL) {
    finish_last_page();
  }
  pdf_release_obj (page_tree_label);
  pdf_release_obj (pages_kids);
  pdf_set_number (page_count_obj, page_count);
  pdf_release_obj (page_count_obj);

  pdf_release_obj (catalog);
  pdf_release_obj (docinfo);
  pdf_release_obj (page_bop);
  pdf_release_obj (page_eop);
  finish_outline();
  finish_dests_tree();
  finish_articles();


  /* Do consistency check on forward references to pages */
  for (i=0; i<page_count; i++) {
    pdf_release_obj (pages[i].page_ref);
    pages[i].page_ref = NULL;
  }
  if (highest_page_ref > page_count)
    fprintf (stderr, "\nWarning:  Nonexistent page referenced\n");
  pdf_out_flush ();
}

