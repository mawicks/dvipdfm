#include "system.h"
#include "pdfobj.h"
#include "pdfspecial.h"
#include "io.h"
#include "epdf.h"


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

double xscale, yscale;

void do_scaling(pdf_obj *media_box, struct xform_info *p)
{ 
  /* Take care of scaling */
  double bbllx, bblly, bburx, bbury;
  bbllx = pdf_number_value (pdf_get_array (media_box, 1));
  bblly = pdf_number_value (pdf_get_array (media_box, 2));
  bburx = pdf_number_value (pdf_get_array (media_box, 3));
  bbury = pdf_number_value (pdf_get_array (media_box, 4));
  xscale = 1.0;
  yscale = 1.0;
  if (p->scale != 0.0) {
    xscale = p->scale;
    yscale = p->scale;
  }
  if (p->xscale != 0.0) {
    xscale = p->xscale;
  }
  if (p->yscale != 0.0) {
    yscale = p->yscale;
  }
  if (p-> width != 0.0 && bbllx != bburx) {
    xscale = p->width / (bburx - bbllx);
    if (p->height == 0.0)
      yscale = xscale;
  }
  if (p->height != 0.0 && bblly != bbury) {
    yscale = p->height / (bbury - bblly);
    if (p->width == 0.0)
      xscale = yscale;
  }
}

int num_xobjects = 0;
struct pdf_obj *make_xform (pdf_obj *this_form_contents, pdf_obj *media_box,  
			    pdf_obj *resources)
{
  struct pdf_obj *contents_ref, *xobj_dict, *tmp1;
  contents_ref = pdf_ref_obj (this_form_contents);  /* Give it a "new" reference */
  xobj_dict = pdf_stream_dict (this_form_contents);
  num_xobjects += 1;
  sprintf (work_buffer, "Fm%d", num_xobjects);
  pdf_doc_add_to_page_xobjects (work_buffer, pdf_link_obj(contents_ref));
  pdf_add_dict (xobj_dict, pdf_new_name ("Name"),
		pdf_new_name (work_buffer));
  pdf_add_dict (xobj_dict, pdf_new_name ("Type"),
		pdf_new_name ("XObject"));
  pdf_add_dict (xobj_dict, pdf_new_name ("Subtype"),
		pdf_new_name ("Form"));
  pdf_add_dict (xobj_dict, pdf_new_name ("BBox"), media_box);
  pdf_add_dict (xobj_dict, pdf_new_name ("FormType"), 
		pdf_new_number(1.0));
  tmp1 = build_scale_array (1, 0, 0, 1, 0, 0);
  pdf_add_dict (xobj_dict, pdf_new_name ("Matrix"), tmp1);
  pdf_add_dict (xobj_dict, pdf_new_name ("Resources"), resources);
  pdf_release_obj(this_form_contents);
  return contents_ref;
}

pdf_obj *pdf_include_page(pdf_obj *trailer, double x_user, double y_user,
			  struct xform_info *p)
{
  pdf_obj *catalog, *page_tree, *kids_ref, *kids;
  pdf_obj *media_box, *resources, *contents, *contents_ref;
  pdf_obj *new_resources, *this_form_contents;
  pdf_obj *tmp1, *tmp2;
  int i;
  char *key;
  /* Now just lookup catalog location */
  /* Deref catalog */

  if ((catalog = pdf_deref_obj(pdf_lookup_dict (trailer,"Root"))) ==
      NULL) {
    fprintf (stderr, "\nCatalog isn't where I expect it.\n");
    return NULL;
  }

  /* Lookup page tree in catalog */
  page_tree = pdf_deref_obj (pdf_lookup_dict (catalog, "Pages"));

  /* Media box and resources can be inherited so start looking for
     them here */
  media_box = pdf_deref_obj (pdf_lookup_dict (page_tree, "MediaBox"));
  resources = pdf_deref_obj (pdf_lookup_dict (page_tree, "Resources"));
  if (resources == NULL) {
    resources = pdf_new_dict();
  }
  while ((kids_ref = pdf_lookup_dict (page_tree, "Kids")) != NULL) {
    kids = pdf_deref_obj (kids_ref);
    pdf_release_obj (page_tree);
    page_tree = pdf_deref_obj (pdf_get_array(kids, 1));
    pdf_release_obj (kids);
    /* Replace MediaBox if it's here */
    tmp1 = pdf_deref_obj(pdf_lookup_dict (page_tree, "MediaBox"));
    if (tmp1 && media_box)
      pdf_release_obj (media_box);
    if (tmp1) 
      media_box = tmp1;
    /* Add resources if they're here */
    tmp1 = pdf_deref_obj (pdf_lookup_dict (page_tree, "Resources"));
    if (tmp1) {
      pdf_merge_dict (tmp1, resources);
      pdf_release_obj (resources);
      resources = tmp1;
    }
  }
  /* At this point, page_tree contains the first page.  media_box and
     resources should also be set. */
  /* Adjust scaling information as necessary */
  do_scaling (media_box, p);
  if ((contents =
       pdf_deref_obj(pdf_lookup_dict(page_tree,"Contents")))==NULL) {
    fprintf (stderr, "\nNo Contents found\n");
    return NULL;
  }
  pdf_release_obj (page_tree);
  /* Arrays of contents must be handled very differently */
  if (contents -> type == PDF_ARRAY) {
    /* For now, we can't handle this so bail out*/
    int i=1;
    if (1) {
      fprintf (stderr, "\nCan't handle content streams with multiple segments\n");
      pdf_release_obj (media_box);
      pdf_release_obj (resources);
      pdf_release_obj (contents);
      return NULL;
    }
    pdf_doc_add_to_page (" q ", 3);
    add_xform_matrix (x_user, y_user, xscale, yscale, p->rotate);
    if (p->depth != 0.0)
      add_xform_matrix (0.0, -p->depth, 1.0, 1.0, 0.0);
    while ((tmp1 = pdf_get_array (contents, i)) != NULL) {
      this_form_contents = pdf_deref_obj (tmp1);
      contents_ref = make_xform (this_form_contents,
				 pdf_link_obj(media_box),
				 pdf_link_obj(resources));
      sprintf (work_buffer, " /Fm%d Do ", num_xobjects);
      pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
      i += 1;
    }
    pdf_doc_add_to_page (" Q ", 3);
    pdf_release_obj (media_box);
    pdf_release_obj (resources);
    pdf_release_obj (contents);
    return (contents_ref);
  } else {
    pdf_doc_add_to_page (" q ", 3);
    add_xform_matrix (x_user, y_user, xscale, yscale, p->rotate);
    if (p->depth != 0.0)
    add_xform_matrix (0.0, -p->depth, 1.0, 1.0, 0.0);
    contents_ref = make_xform (contents, media_box, resources);
    sprintf (work_buffer, " /Fm%d Do ", num_xobjects);
    pdf_doc_add_to_page (work_buffer, strlen(work_buffer));
    pdf_doc_add_to_page (" Q ", 3);
  }
  return (contents_ref);
}
