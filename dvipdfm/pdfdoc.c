/*  $Header: /home/mwicks/Projects/Gaspra-projects/cvs2darcs/Repository-for-sourceforge/dvipdfm/pdfdoc.c,v 1.11 1998/12/03 20:19:48 mwicks Exp $

    This is dvipdf, a DVI to PDF translator.
    Copyright (C) 1998  by Mark A. Wicks

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
    
    The author may be contacted via the e-mail address

	mwicks@kettering.edu
*/

	
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "c-auto.h"
#include "system.h"
#include "pdflimits.h"
#include "pdfobj.h"
#include "error.h"
#include "mem.h"
#include "pdfdoc.h"
#include "pdfdev.h"
#include "numbers.h"
#include "mfileio.h"

static pdf_obj *catalog = NULL;
static pdf_obj *docinfo = NULL;
static pdf_obj *page_tree = NULL, *page_tree_label = NULL, *pages_kids = NULL;

int outline_depth=0;
static struct 
{
  int kid_count;
  pdf_obj *entry;
} outline[MAX_OUTLINE_DEPTH];


static pdf_obj *current_page_resources = NULL;
static pdf_obj *page_count_obj = NULL, *this_page_contents = NULL;
static pdf_obj *glob_page_bop, *glob_page_eop;
static pdf_obj *this_page_bop = NULL, *this_page_eop = NULL;
static pdf_obj *this_page_beads = NULL;
static pdf_obj *this_page_annots = NULL;
static pdf_obj *this_page_xobjects = NULL, *this_page_fonts = NULL;;
static pdf_obj *tmp1, *tmp2;

static int page_count = 0;
static struct {
  pdf_obj *page_dict;
  pdf_obj *page_ref;
} pages[MAX_PAGES];

static void start_page_tree (void);
static void create_catalog (void);
static void start_current_page_resources (void);
static void finish_building_page_tree(void);
static void start_name_tree(void);
static void start_dests_tree(void);
static void finish_dests_tree(void);
static void finish_pending_xobjects(void);
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

static void start_page_tree (void)
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
		pdf_new_name ("Pages"),
		pdf_link_obj (page_tree_label));
  glob_page_bop = pdf_new_stream();
  glob_page_eop = pdf_new_stream();
  return;
}

void pdf_doc_bop (char *string, unsigned length)
{
  if (length > 0)
    pdf_add_stream (glob_page_bop, string, length);
}

void pdf_doc_this_bop (char *string, unsigned length)
{
  if (length > 0)
    pdf_add_stream (this_page_bop, string, length);
}

void pdf_doc_this_eop (char *string, unsigned length)
{
  if (length > 0)
    pdf_add_stream (this_page_eop, string, length);
}


void pdf_doc_eop (char *string, unsigned length)
{
  if (length > 0)
    pdf_add_stream (glob_page_eop, string, length);
}

static void start_outline_tree (void)
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
		pdf_new_name ("Outlines"),
		pdf_ref_obj(outline[outline_depth].entry));
  return;
}

static pdf_obj *names_dict;

static void start_name_tree (void)
{
  if (debug) {
    fprintf (stderr, "(start_name_tree)");
  }
  names_dict = pdf_new_dict ();
  pdf_add_dict (catalog,
		pdf_new_name ("Names"),
		pdf_ref_obj (names_dict));
}

static char *asn_date (void)
{
#ifndef HAVE_TIMEZONE
  #ifdef TM_GM_TOFF
     #define timezone (bdtime->gm_toff)
  #else
     #define timezone 0
#endif /* TM_GM_TOFF */
#endif /* HAVE_TIMEZONE */
  static char date_string[22];
  time_t current_time;
  struct tm *bd_time;
  if (debug) {
    fprintf (stderr, "(asn_date)");
  }
  time(&current_time);
  bd_time = localtime(&current_time);
  sprintf (date_string, "D:%04d%02d%02d%02d%02d%02d%0+3ld'%02ld'",
	   bd_time -> tm_year+1900, bd_time -> tm_mon+1, bd_time -> tm_mday,
	   bd_time -> tm_hour, bd_time -> tm_min, bd_time -> tm_sec,
	   -timezone/3600, timezone%3600);
  return date_string;
}

#define BANNER "dvipdfm %s, Copyright (C) 1998, by Mark A. Wicks"
static void create_docinfo (void)
{
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
		pdf_new_name ("Producer"),
		pdf_new_string (banner, strlen (banner)));
  release (banner);
  
  time_string = asn_date();
  pdf_add_dict (docinfo, 
		pdf_new_name ("CreationDate"),
		pdf_new_string (time_string, strlen (time_string)));
  pdf_set_info (docinfo);
  return;
}

void pdf_doc_merge_with_docinfo (pdf_obj *dictionary)
{
  pdf_merge_dict (docinfo, dictionary);
}

void pdf_doc_merge_with_catalog (pdf_obj *dictionary)
{
  pdf_merge_dict (catalog, dictionary);
}

static void create_catalog (void)
{
  if (debug) {
    fprintf (stderr, "(create_catalog)");
  }
  catalog = pdf_new_dict ();
  pdf_set_root (catalog);
  /* Create /Type attribute */
  pdf_add_dict (catalog,
		pdf_new_name ("Type"),
		pdf_new_name("Catalog"));
 /* Create only those parts of the page tree required for the catalog.
    That way, the rest of the page tree can be finished at any time */
  start_page_tree(); 
  /* Likewise for outline tree */
  start_outline_tree ();
  start_name_tree();
  start_dests_tree();
  start_articles();
  return;
}

static void start_current_page_resources (void)
{
  /* work on resources to put in Pages */
  if (debug) {
    fprintf (stderr, "(start_current_page_resources)");
  }
  current_page_resources = pdf_new_dict ();
  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, pdf_new_name ("PDF"));
  pdf_add_array (tmp1, pdf_new_name ("Text"));
  pdf_add_array (tmp1, pdf_new_name ("ImageC"));
  pdf_add_dict (current_page_resources,
		pdf_new_name ("ProcSet"),
		tmp1);
  this_page_fonts = pdf_new_dict ();
  pdf_add_dict (current_page_resources, 
		pdf_new_name ("Font"),
		pdf_ref_obj (this_page_fonts));
  this_page_xobjects = pdf_new_dict ();
  pdf_add_dict (current_page_resources,
		pdf_new_name ("XObject"),
		pdf_ref_obj (this_page_xobjects));
  return;
}

void pdf_doc_add_to_page_fonts (const char *name, pdf_obj
				   *resource)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_fonts)");
  }
  pdf_add_dict (this_page_fonts,
		pdf_new_name (name), resource);
}

void pdf_doc_add_to_page_xobjects (const char *name, pdf_obj
				   *resource)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_xojects)");
  }
  pdf_add_dict (this_page_xobjects,
		pdf_new_name (name), 
		resource);
}


void pdf_doc_add_to_page_resources (const char *name, pdf_obj *resource)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_resources)");
  }
  pdf_add_dict (current_page_resources,
		pdf_new_name (name), 
		resource);
}

void pdf_doc_add_to_page_annots (pdf_obj *annot)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_add_to_page_annots)");
  }
  pdf_add_array (this_page_annots,
		 annot);
}

static void finish_building_page_tree(void)
{
  /* Back to work on that page tree */
  if (debug) {
    fprintf (stderr, "(finish_bulding_page_tree)");
  }
  pdf_add_dict (page_tree,
		pdf_new_name ("Type"),
		pdf_new_name ("Pages"));
  page_count_obj = pdf_new_number (0);
  pdf_add_dict (page_tree,
		pdf_new_name ("Count"),
		pdf_ref_obj(page_count_obj));
  pages_kids = pdf_new_array ();
  pdf_add_dict (page_tree,
		pdf_new_name ("Kids"),
		pdf_ref_obj (pages_kids));
  return;
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
    pdf_add_dict (outline[i].entry,
		  pdf_new_name ("Last"),
		  pdf_ref_obj (outline[i+1].entry));
    if (i > 0) 
      tmp1 = pdf_new_number (-outline[i].kid_count);
    else
      tmp1 = pdf_new_number (outline[i].kid_count);

    pdf_add_dict (outline[i].entry,
		  pdf_new_name ("Count"),
		  tmp1);
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
  if (verbose)
    fprintf (stderr, "(finish_outline)");
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
  pdf_merge_dict (new_entry, dict);
  /* Tell it where its parent is */
  pdf_add_dict (new_entry,
		pdf_new_name ("Parent"),
		pdf_ref_obj (outline[outline_depth-1].entry));
  /* Give mom and dad the good news */
  outline[outline_depth-1].kid_count += 1;

  /* Is this the first entry at this depth? */
  if (outline[outline_depth].entry == NULL) {
    /* Is so, tell the parent we are first born */
    pdf_add_dict (outline[outline_depth-1].entry,
		  pdf_new_name ("First"),
		  pdf_ref_obj (new_entry));
  }
  else {
    /* Point us back to sister */
    pdf_add_dict (new_entry,
		  pdf_new_name ("Prev"),
		  pdf_ref_obj (outline[outline_depth].entry));
    /* Point our elder sister toward us */
    pdf_add_dict (outline[outline_depth].entry,
		  pdf_new_name ("Next"),
		  pdf_ref_obj (new_entry));
    /* Bye-Bye sis */
    pdf_release_obj (outline[outline_depth].entry);
  }
  outline[outline_depth].entry = new_entry;
  /* Just born, so don't have any kids */
  outline[outline_depth].kid_count = 0;
}

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
		pdf_new_name ("Dests"),
		pdf_ref_obj (dests_dict));
}


#define MIN(a,b) ((a)<(b)? (a): (b))

static int CDECL cmp_dest (const void *d1, const void *d2)
{
  unsigned length;
  int tmp;
  length = MIN (((dest_entry *) d1) -> length, ((dest_entry *) d2) ->
		length);
  if ((tmp = memcmp (((dest_entry *) d1) -> name, ((dest_entry *) d2)
		      -> name, length)) != 0)
    return tmp;
  if (((dest_entry *) d1) -> length == ((dest_entry *) d2) -> length)
    return 0;
  return (((dest_entry *) d1) -> length < ((dest_entry *) d2) -> length ? -1 : 1 );
}

static void finish_dests_tree (void)
{
  pdf_obj *kid, *name_array;
  int i;
  if (number_dests <= 0){
    pdf_release_obj (dests_dict);
    return;
  }
  name_array = pdf_new_array ();
  qsort(dests, number_dests, sizeof(dests[0]), cmp_dest);
  for (i=0; i<number_dests; i++) {
    pdf_add_array (name_array, pdf_new_string (dests[i].name,
					       dests[i].length));
    pdf_add_array (name_array, pdf_link_obj (dests[i].array));
  }
  kid = pdf_new_dict ();
  pdf_add_dict (kid,
		pdf_new_name ("Names"),
		name_array);
  tmp1 = pdf_new_array();
  pdf_add_array (tmp1, pdf_new_string (dests[0].name,
 				       dests[0].length));
  pdf_add_array (tmp1, pdf_new_string (dests[number_dests-1].name,
				       dests[number_dests-1].length));
  pdf_add_dict (kid,
		pdf_new_name ("Limits"),
		tmp1);
  tmp2 = pdf_new_array();
  pdf_add_array (tmp2, pdf_ref_obj (kid));
  pdf_add_dict (dests_dict,
		pdf_new_name ("Kids"),
		tmp2);
  pdf_release_obj (kid);
  pdf_release_obj (dests_dict);
}


void pdf_doc_add_dest (char *name, unsigned length, pdf_obj *array )
{
  if (number_dests >= MAX_DESTS) {
    ERROR ("pdf_doc_add_dest:  Too many named destinations\n");
  }
  dests[number_dests].name = NEW (length, char);
  memcpy (dests[number_dests].name, name, length);
  dests[number_dests].length = length;
  dests[number_dests].array = pdf_ref_obj (array);
  number_dests++;
}

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
		pdf_new_name ("Threads"),
		pdf_ref_obj (articles_array));
}

void pdf_doc_start_article (char *name, pdf_obj *info)
{
  if (number_articles >= MAX_ARTICLES) {
    ERROR ("pdf_doc_add_article:  Too many articles\n");
  }
  articles[number_articles].name = NEW (strlen(name)+1, char);
  strcpy (articles[number_articles].name, name);
  articles[number_articles].info = info;
  articles[number_articles].first = NULL;
  articles[number_articles].last = NULL;
  /* Start dictionary for this article even though we can't finish it
     until we get the first bead */
  articles[number_articles].this = pdf_new_dict();
  number_articles++;
  return;
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
		  pdf_new_name ("F"),
		  pdf_ref_obj (articles[i].first));
    /* Next add pointer to its Info dictionary */
    pdf_add_dict (articles[i].this,
		  pdf_new_name ("I"),
		  pdf_ref_obj (articles[i].info));
    /* Point first bead to parent article */
    pdf_add_dict (partial_dict,
		  pdf_new_name ("T"),
		  pdf_ref_obj (articles[i].this));
    /* Ship it out and forget it */
    pdf_add_array (articles_array, pdf_ref_obj (articles[i].this));
    pdf_release_obj (articles[i].this);
    articles[i].this = NULL;
  } else {
    /* Link it in... */
    /* Point last object to this one */
    pdf_add_dict (articles[i].last,
		  pdf_new_name ("N"),
		  pdf_ref_obj (partial_dict));
    /* Point this one to last */
    pdf_add_dict (partial_dict,
		  pdf_new_name ("V"),
		  pdf_ref_obj (articles[i].last));
    pdf_release_obj (articles[i].last);
  }
  articles[i].last = partial_dict;
  pdf_add_array (this_page_beads,
		 pdf_ref_obj (partial_dict));
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
		  pdf_new_name ("N"),
		  pdf_ref_obj (articles[i].first));
    pdf_add_dict (articles[i].first,
		  pdf_new_name ("V"),
		  pdf_ref_obj (articles[i].last));
    pdf_release_obj (articles[i].first);
    pdf_release_obj (articles[i].last);
    pdf_release_obj (articles[i].info);
  }
}


static void finish_last_page ()
{
  finish_pending_xobjects();
  /* Flush this page */
  /* Page_count is the index of the current page, starting at 1 */
  if (page_count > 0) {
    /* Write out MediaBox as the very last thing we do on the
       previous page */
    tmp1 = pdf_new_array ();
    pdf_add_array (tmp1, pdf_new_number (0));
    pdf_add_array (tmp1, pdf_new_number (0));
    pdf_add_array (tmp1, pdf_new_number
		   (ROUND(dev_page_width(),1.0)));
    pdf_add_array (tmp1, pdf_new_number (ROUND(dev_page_height(),1.0)));
    pdf_add_dict (pages[page_count-1].page_dict,
		  pdf_new_name ("MediaBox"),
		  tmp1);
    pdf_release_obj (pages[page_count-1].page_dict);
    pages[page_count-1].page_dict = NULL;
  }
  if (debug) {
    fprintf (stderr, "(finish_last_page)");
  }
  if (this_page_bop != NULL) {
    pdf_release_obj (this_page_bop);
    this_page_bop = NULL;
  }
  if (this_page_eop != NULL) {
    pdf_release_obj (this_page_eop);
    this_page_eop = NULL;
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
  if (this_page_fonts != NULL) {
    pdf_release_obj (this_page_fonts);
    this_page_fonts = NULL;
  }
  if (this_page_xobjects != NULL) {
    pdf_release_obj (this_page_xobjects);
    this_page_xobjects = NULL;
  }
}

pdf_obj *pdf_doc_current_page_resources (void)
{
  return current_page_resources;
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

pdf_obj *pdf_doc_names (void)
{
  return names_dict;
}

pdf_obj *pdf_doc_page_tree (void)
{
  return page_tree;
}

pdf_obj *pdf_doc_catalog (void)
{
  return catalog;
}

pdf_obj *pdf_doc_this_page (void)
{
  if (page_count <= 0) {
    ERROR ("Reference to current page, but no pages have been started yet");
  }
  fprintf (stderr, "pdf_doc_this_page: page_count = %d, ref=%p\n",
	   page_count, pages[page_count-1].page_dict);
  return pages[page_count-1].page_dict;
}

pdf_obj *pdf_doc_this_page_ref (void)
{
  if (page_count <= 0) {
    ERROR ("Reference to current page, but no pages have been started yet");
  }
  return pdf_doc_ref_page(page_count);
}

pdf_obj *pdf_doc_prev_page_ref (void)
{
  if (page_count <= 0) {
    ERROR ("Reference to previous page, but no pages have been started yet");
  }
  return pdf_doc_ref_page(page_count>1?page_count-1:1);
}

pdf_obj *pdf_doc_next_page_ref (void)
{
  if (page_count <= 0) {
    ERROR ("Reference to previous page, but no pages have been started yet");
  }
  return pdf_doc_ref_page(page_count+1);
}

void pdf_doc_new_page (void)
{
  if (debug) {
    fprintf (stderr, "(pdf_doc_new_page)");
  }
  if (page_count >= MAX_PAGES) 
    ERROR ("Too many pages in pdf_doc_new_page\n");
  
  if (this_page_contents != NULL) {
    finish_last_page();
  }
  this_page_bop = pdf_new_stream();
  this_page_eop = pdf_new_stream();
  /* Was this page already instantiated by a forward reference to it? */
  if (pages[page_count].page_dict == NULL) {
    /* If not, create it. */
    pages[page_count].page_dict = pdf_new_dict ();
    /* and reference it */
    pages[page_count].page_ref = pdf_ref_obj(pages[page_count].page_dict);
  }
  pdf_add_array (pages_kids,
		 pdf_link_obj(pages[page_count].page_ref));

  pdf_add_dict (pages[page_count].page_dict,
		pdf_new_name ("Type"),
		pdf_new_name ("Page"));
  pdf_add_dict (pages[page_count].page_dict,
		pdf_new_name ("Parent"),
		pdf_link_obj (page_tree_label));
  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, pdf_ref_obj (glob_page_bop));
  pdf_add_array (tmp1, pdf_ref_obj (this_page_bop));
  /* start the contents stream for the new page */
  this_page_contents = pdf_new_stream();
  pdf_add_array (tmp1, pdf_ref_obj (this_page_contents));
  pdf_add_array (tmp1, pdf_ref_obj (this_page_eop));
  pdf_add_array (tmp1, pdf_ref_obj (glob_page_eop));
  pdf_add_dict (pages[page_count].page_dict,
		pdf_new_name ("Contents"),
		tmp1);

  this_page_annots = pdf_new_array ();
  pdf_add_dict (pages[page_count].page_dict,
		pdf_new_name ("Annots"),
		pdf_ref_obj (this_page_annots));
  start_current_page_resources();
  pdf_add_dict (pages[page_count].page_dict,
		pdf_new_name ("Resources"),
		pdf_ref_obj (current_page_resources));
  this_page_beads = pdf_new_array();
  pdf_add_dict (pages[page_count].page_dict,
		pdf_new_name ("B"),
		pdf_ref_obj (this_page_beads));
  /* Contents are still available as this_page_contents until next
     page is started */
  /* Even though the page is gone, a Reference to this page is kept
     until program ends */
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
  finish_building_page_tree();	/* start_page_tree was called by create_catalog */
}

void pdf_doc_creator (char *s)
{
  pdf_add_dict (docinfo, pdf_new_name ("Creator"),
		pdf_new_string (s, strlen(s)));
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
  /* Following things were kept around so user can add dictionary
     items */
  pdf_release_obj (catalog);
  pdf_release_obj (docinfo);
  pdf_release_obj (page_tree);
  pdf_release_obj (names_dict);
  pdf_release_obj (glob_page_bop);
  pdf_release_obj (glob_page_eop);
  finish_outline();
  finish_dests_tree();
  finish_articles();

  /* Do consistency check on forward references to pages */
  for (i=0; i<page_count; i++) {
    pdf_release_obj (pages[i].page_ref);
    pages[i].page_ref = NULL;
  }
  if (highest_page_ref > page_count) {
    fprintf (stderr, "\nWarning:  Nonexistent page(s) referenced\n");
    fprintf (stderr, "          (PDF file may not work right)\n");
  }
  pdf_out_flush ();
}


static pdf_obj *build_scale_array (int a, int b, int c, int d, int e, int f)
{
  pdf_obj *result;
  result = pdf_new_array();
  pdf_add_array (result, pdf_new_number (a));
  pdf_add_array (result, pdf_new_number (b));
  pdf_add_array (result, pdf_new_number (c));
  pdf_add_array (result, pdf_new_number (d));
  pdf_add_array (result, pdf_new_number (e));
  pdf_add_array (result, pdf_new_number (f));
  return result;
}

/* All this routine does is give the form a name
   and add a unity scaling matrix. It fills
   in required fields.  The caller must initialize
   the stream */
int num_xobjects = 0;
void doc_make_form_xobj (pdf_obj *this_form_contents, pdf_obj *bbox,
			    pdf_obj *resources)
{
  pdf_obj *xobj_dict, *tmp1;
  xobj_dict = pdf_stream_dict (this_form_contents);
  num_xobjects += 1;
  sprintf (work_buffer, "Fm%d", num_xobjects);
  pdf_add_dict (xobj_dict, pdf_new_name ("Name"),
		pdf_new_name (work_buffer));
  pdf_add_dict (xobj_dict, pdf_new_name ("Type"),
		pdf_new_name ("XObject"));
  pdf_add_dict (xobj_dict, pdf_new_name ("Subtype"),
		pdf_new_name ("Form"));
  pdf_add_dict (xobj_dict, pdf_new_name ("BBox"), bbox);
  pdf_add_dict (xobj_dict, pdf_new_name ("FormType"), 
		pdf_new_number(1.0));
  tmp1 = build_scale_array (1, 0, 0, 1, 0, 0);
  pdf_add_dict (xobj_dict, pdf_new_name ("Matrix"), tmp1);
  pdf_add_dict (xobj_dict, pdf_new_name ("Resources"), resources);
  return;
}

static pdf_obj *save_page_contents, *save_page_fonts;
static pdf_obj *save_page_xobjects, *save_page_resources;
static xobject_pending = 0;

pdf_obj *begin_form_xobj (double bbllx, double bblly, double bburx,
			  double bbury)
{
  pdf_obj *bbox;
  if (xobject_pending) {
    fprintf (stderr, "\nCannot next form XObjects\n");
    return NULL;
  }
  xobject_pending = 1;
  /* This is a hack.  We basically treat an xobj as a separate mini
     page unto itself.  Save all the page structures and reinitialize them. */
  save_page_resources = current_page_resources;
  save_page_xobjects = this_page_xobjects;
  save_page_fonts = this_page_fonts;
  save_page_contents = this_page_contents;
  start_current_page_resources(); /* Starts current_page_resources,
				     this_page_xobjects, and this_page_fonts */
  this_page_contents = pdf_new_stream ();
  /* Make a bounding box for this Xobject */
  bbox = pdf_new_array ();
  pdf_add_array (tmp1, pdf_new_number (bbllx));
  pdf_add_array (tmp1, pdf_new_number (bblly));
  pdf_add_array (tmp1, pdf_new_number (bburx));
  pdf_add_array (tmp1, pdf_new_number (bbury));
  /* Resource is already made, so call doc_make_form_xobj() */
  doc_make_form_xobj (this_page_contents, bbox,
		      pdf_ref_obj(current_page_resources));
  return pdf_link_obj (this_page_contents);
}

void end_form_xobj (void)
{
  xobject_pending = 0;
  pdf_release_obj (current_page_resources);
  pdf_release_obj (this_page_xobjects);
  pdf_release_obj (this_page_fonts);
  pdf_release_obj (this_page_contents);
  current_page_resources = save_page_resources;
  this_page_xobjects = save_page_xobjects;
  this_page_fonts = save_page_fonts;
  this_page_contents = save_page_contents;
  return;
}

void finish_pending_xobjects (void)
{
  if (xobject_pending) {
    fprintf (stderr, "\nFinishing a pending form XObject at end of page\n");
    end_form_xobj();
  }
}
