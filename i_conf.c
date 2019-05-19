/*
 * Author: Catalin(ux) M. BOIE
 * Description: Configuration file loader
 */

#include <i_conf.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


struct ip2clue_conf *ip2clue_conf_load(const char *file)
{
	FILE *f;
	char *r;
	struct ip2clue_conf *ret = NULL, *cur, *prev = NULL;
	char buf[4096], *p, *q, *e;

	f = fopen(file, "r");
	if (!f)
		return NULL;

	while (1) {
		r = fgets(buf, sizeof(buf), f);
		if (!r)
			break;

		if (strlen(buf) == 0)
			continue;

		if (buf[0] == '#')
			continue;

		p = buf;
		q = strchr(p, '=');
		if (!q)
			continue;
		*q = '\0';
		q += 1;
		while ((*q == ' ') || (*q == '\t'))
			q++;
		e = q + strlen(q) - 1;
		while ((*e == ' ') || (*e == '\t') || (*e == '\r') || (*e == '\n')) {
			*e = '\0';
			e--;
		}

		while ((*p == ' ') || (*p == '\t'))
			p++;
		e = p + strlen(p) - 1;
		while ((*e == ' ') || (*e == '\t') || (*e == '\r') || (*e == '\n')) {
			*e = '\0';
			e--;
		}

		cur = (struct ip2clue_conf *) malloc(sizeof(struct ip2clue_conf));
		if (!cur)
			goto out_close;
		if (ret == NULL)
			ret = cur;

		if (prev)
			prev->next = cur;

		cur->l = strdup(p);
		cur->r = strdup(q);
		cur->next = NULL;

		prev = cur;
	}

	out_close:
	fclose(f);

	return ret;
}

char *ip2clue_conf_get(const struct ip2clue_conf *c, const char *l)
{
	while (c) {
		if (strcmp(l, c->l) == 0)
			return c->r;
		c = c->next;
	}

	return NULL;
}

unsigned long ip2clue_conf_get_ul(const struct ip2clue_conf *c, const char *l,
	const unsigned int base)
{
	char *r;

	r = ip2clue_conf_get(c, l);
	if (!r)
		return 0;

	return strtoul(r, NULL, base);
}

void ip2clue_conf_free(struct ip2clue_conf *c)
{
	struct ip2clue_conf *next;

	while (c) {
		free(c->l);
		free(c->r);
		next = c->next;
		free(c);
		c = next;
	}
}
