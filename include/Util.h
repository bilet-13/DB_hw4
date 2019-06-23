#ifndef DB_UTIL_H
#define DB_UTIL_H
#include "Command.h"
#include "Table.h"

typedef struct State {
    int saved_stdout;
} State_t;

State_t* new_State();
void print_prompt(State_t *state);
void print_join(User_t *user,Like_t *like ,SelectArgs_t *sel_args);
void print_user(User_t *user, SelectArgs_t *sel_args);
void print_like(Like_t *like, SelectArgs_t *sel_args);

void print_joins(Table_t *user_table, Table_like_t *like_table,
    int *idxList, size_t idxListLen , Command_t *cmd, join_condiction_t* join_condiction);

void print_users(Table_t *table, int *idxList, size_t idxListLen, Command_t *cmd);
void print_likes(Table_like_t *table, Command_t *cmd);
int parse_input(char *input, Command_t *cmd);
void handle_builtin_cmd(Table_t *table, Command_t *cmd, State_t *state);
int handle_query_cmd(Table_t *table, Table_like_t *like_table, Command_t *cmd);
int handle_insert_cmd(Table_t *table,Table_like_t *like_table, Command_t *cmd);
int handle_user_select_cmd(Table_t *table,Table_like_t *like_table, Command_t *cmd, unsigned int cmd_i);
int handle_like_select_cmd(Table_like_t *like_table, Command_t *cmd, unsigned int cmd_i);
void print_help_msg();
int check_join_condiction(User_t *user,Like_t *like,join_condiction_t *condiction);
join_condiction_t  handle_join_cmd(Command_t *cmd,size_t len);
int handle_update_cmd(Table_t *table, Command_t *cmd);
int handle_delete_cmd(Table_t *table, Command_t *cmd);
#endif
