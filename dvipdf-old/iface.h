#ifndef IFACE_H
#define IFACE_H

void dev_init (char *outputfile);

unsigned dev_tell_xdpi(void);

unsigned dev_tell_ydpi(void);

void dev_locate_font (char *name, unsigned long tex_font_id, double ptsize);

void dev_bop (void);

void dev_eop (void);

void dev_change_to_font (long tex_font_id);

void dev_set_char (unsigned ch);

void dev_rule (long x, long y, long width, long height);

void dev_right (long length);

void dev_gotoxy (long x, long y);

#endif /* IFACE_H */








