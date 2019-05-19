#ifndef IP2CLUE_CONF_H
#define IP2CLUE_CONF_H 1

#include <i_config.h>

struct ip2clue_conf
{
	char *l;
	char *r;
	struct ip2clue_conf *next;
};


extern struct ip2clue_conf	*ip2clue_conf_load(const char *file);
extern char			*ip2clue_conf_get(const struct ip2clue_conf *c,
					const char *l);
extern unsigned long		ip2clue_conf_get_ul(const struct ip2clue_conf *c,
					const char *l, const unsigned int base);
extern void			ip2clue_conf_free(struct ip2clue_conf *c);

#endif
