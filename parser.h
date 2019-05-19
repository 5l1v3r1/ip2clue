#ifndef IP2CLUE_PARSER_H
#define IP2CLUE_PARSER_H 1

#include <i_config.h>

#include <i_types.h>

extern int	ip2clue_parse_file(struct ip2clue_db *db,
			const char *file_name,
			const char *format);

extern void	ip2clue_list_init(struct ip2clue_list *list);
extern void	ip2clue_list_destroy(struct ip2clue_list *db);

extern void	ip2clue_list_stats(char *out,
			const size_t out_size, struct ip2clue_list *list);

extern int	ip2clue_list_clone(struct ip2clue_list *dst,
			struct ip2clue_list *src);

extern int	ip2clue_list_replace(struct ip2clue_list *dst,
			struct ip2clue_list *src);

extern int	ip2clue_list_refresh(struct ip2clue_list *dst,
			struct ip2clue_list *src, const char *dir,
			const char *options);

extern int	ip2clue_list_load(struct ip2clue_list *list,
			const char *dir, const char *options);

#endif