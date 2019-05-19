#ifndef IP2CLUE_I_UTIL_H
#define IP2CLUE_I_UTIL_H 1

#include <i_config.h>

#include <stdlib.h>

#include <i_types.h>

extern char		ip2clue_error[256];

extern char		*ip2clue_strerror(void);

extern int		ip2clue_addr_v6(char *out, size_t out_size,
				const unsigned int *a);

extern int		ip2clue_split(struct ip2clue_split *s, const char *line,
				const char *sep);

extern long		ip2clue_file_lines(const char *file);

extern void		ip2clue_destroy(struct ip2clue_db *db);

extern void		ip2clue_print_extra(char *out, size_t out_size,
				const struct ip2clue_extra *e);

/* v4 */
extern struct ip2clue_cell_v4	*ip2clue_search_v4(struct ip2clue_db *db,
					const char *ip);
extern struct ip2clue_cell_v4	*ip2clue_list_search_v4(struct ip2clue_list *list,
					const char *ip);
extern void		ip2clue_dump_cell_v4(char *out, size_t out_size,
				struct ip2clue_cell_v4 *cell); /* TODO */

/* v6 */
extern struct ip2clue_cell_v6	*ip2clue_search_v6(struct ip2clue_db *db,
					const char *ip);
extern struct ip2clue_cell_v6   *ip2clue_list_search_v6(struct ip2clue_list *list,
					const char *ip);
extern void		ip2clue_dump_cell_v6(char *out, size_t out_size,
				struct ip2clue_cell_v6 *cell);

/* common */
extern int		ip2clue_list_search(struct ip2clue_list *list,
				char *out, const unsigned int out_size,
				const char *format, const char *ip);
#endif
