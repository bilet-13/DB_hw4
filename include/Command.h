#ifndef COMMAND_H
#define COMMAND_H

enum { 
    UNRECOG_CMD,
    BUILT_IN_CMD,
    QUERY_CMD,
};

enum {
    INSERT_CMD = 100,
    SELECT_CMD,
};

enum {
        STAR = 1,
        ID,
        NAME,
        EMAIL,
        AGE,
    };
    
enum{
    AVG = 10,
    COUNT,
    SUM
};

typedef struct where_conditon{
    int field;
    char op[3];
    char content[256];
    size_t com_cur_len;//The position of array in command which is unused
} where_conditon_t;

typedef struct {
    char name[256];
    int len;
    unsigned char type;
} CMD_t;

extern CMD_t cmd_list[];

typedef struct SelectArgs {
    char **fields;
    size_t fields_len;
    int offset;
    int limit;
} SelectArgs_t;

typedef union {
    SelectArgs_t sel_args;
} CmdArg_t;

typedef struct Command {
    unsigned char type;
    char **args;
    size_t args_len;
    size_t args_cap;
    CmdArg_t cmd_args;
} Command_t;

where_conditon_t handle_where_cmd(Command_t *cmd,size_t len);
Command_t* new_Command();
int add_Arg(Command_t *cmd, const char *arg);
int add_select_field(Command_t *cmd, const char *argument);
void cleanup_Command(Command_t *cmd);

#endif
