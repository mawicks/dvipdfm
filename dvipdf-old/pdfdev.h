#ifndef PDFDEV_H
#define PDFDEV_H

void dev_init (char *outputfile);

void dev_close (void);

unsigned long dev_tell_xdpi(void);

unsigned long dev_tell_ydpi(void);

void dev_locate_font (char *name, unsigned long tex_font_id,
		      int tfm_font_id, double ptsize);

void dev_bop (void);

void dev_eop (void);

void dev_select_font (long tex_font_id);

void dev_set_char (unsigned ch, double width);

void dev_set_page_size (double width, double height);

void dev_rule (double width, double height);

void dev_moveright (double length);

void dev_moveto (double x, double y);

void dev_set_bop (char *s, unsigned length);
void dev_set_eop (char *s, unsigned length);

double dev_tell_x (void);
double dev_tell_y (void);

#endif /* PDFDEV_H */
