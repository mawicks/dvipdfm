#include <stdio.h>
#include "pdfobj.h"

static pdf_obj *catalog = NULL;
static pdf_obj *page_tree = NULL, *page_tree_label = NULL, *pages_kids = NULL;
static pdf_obj *current_page_resources = NULL;
static pdf_obj *page_count_obj = NULL, *this_page_contents = NULL;
static pdf_obj *tmp1, *tmp2, *tmp3;

static int page_count = 0;

static start_page_tree (void);
static create_catalog (void);
static start_current_page_resources (void);
static finish_page_tree(void);

static start_page_tree (void)
{
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
}

static create_catalog (void)
{
  catalog = pdf_new_dict ();
  pdf_set_root (catalog);
  /* Create /Type attribute */
  pdf_add_dict (catalog,
		tmp1 = pdf_new_name ("Type"),
		tmp2 = pdf_new_name("Catalog"));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  start_page_tree();
  /* The catalog's done.  Ship it out */
  pdf_release_obj (catalog);
}

static start_current_page_resources (void)
{
  /* work on resources to put in Pages */
  current_page_resources = pdf_new_dict ();
  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, tmp2 = pdf_new_name ("PDF"));
  pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_name ("Text"));
  pdf_release_obj (tmp2);
  pdf_add_dict (current_page_resources,
		tmp2 = pdf_new_name ("ProcSet"),
		tmp1);
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
}


static finish_page_tree(void)
{
  /* Back to work on that page tree */
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

static void finish_last_page ()
{
  if (this_page_contents != NULL) {
    pdf_release_obj (this_page_contents);
    this_page_contents = NULL;
  }
  if (current_page_resources != NULL) {
    pdf_release_obj (current_page_resources);
    current_page_resources = NULL;
  }
}
    
void pdf_doc_new_page (unsigned width, unsigned height) /* Units are points */
{
  pdf_obj *this_page;
  if (this_page_contents != NULL) {
    finish_last_page();
  }
  
  this_page = pdf_new_dict ();
  page_count += 1;
  pdf_add_array (pages_kids,
		 tmp1 = pdf_ref_obj (this_page));
  pdf_release_obj (tmp1);
  pdf_add_dict (this_page,
		tmp1 = pdf_new_name ("Type"),
		tmp2 = pdf_new_name ("Page"));
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);

  tmp1 = pdf_new_array ();
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (0)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (width)); pdf_release_obj (tmp2);
  pdf_add_array (tmp1, tmp2 = pdf_new_number (height)); pdf_release_obj (tmp2);
  pdf_add_dict (this_page,
		tmp2 = pdf_new_name ("MediaBox"),
		tmp1);
  pdf_release_obj (tmp1);
  pdf_release_obj (tmp2);
  pdf_add_dict (this_page, tmp1 = pdf_new_name ("Parent"),
		page_tree_label);
  pdf_release_obj (tmp1);
  this_page_contents = pdf_new_stream();
  pdf_add_dict (this_page, tmp1 = pdf_new_name ("Contents"),
		tmp1 = pdf_ref_obj (this_page_contents));
  pdf_release_obj (tmp1);
  start_current_page_resources();
  pdf_add_dict (this_page, tmp1 = pdf_new_name ("Resources"),
		tmp1 = pdf_ref_obj (current_page_resources));
  pdf_release_obj (tmp1);
  /* Flush this page */
  pdf_release_obj (this_page);
  /* Contents are still available as this_page_contents */
}

void pdf_doc_add_to_page (char *buffer, unsigned length)
{
  pdf_add_stream (this_page_contents, buffer, length);
}

void pdf_doc_init (char *filename) 
{
  pdf_out_init (filename);
  create_catalog ();
  finish_page_tree();	/* start_page_tree was called by create_catalog */
}

void pdf_doc_finish ()
{
  if (this_page_contents != NULL) {
    finish_last_page();
  }
  pdf_release_obj (page_tree_label);
  pdf_release_obj (pages_kids);
  pdf_set_number (page_count_obj, page_count);
  pdf_release_obj (page_count_obj);
  pdf_out_flush ();
}
