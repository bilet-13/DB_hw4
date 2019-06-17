#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "Util.h"
#include "Command.h"
#include "Table.h"
#include "SelectState.h"

///
/// Allocate State_t and initialize some attributes
/// Return: ptr of new State_t
///
State_t* new_State() {
    State_t *state = (State_t*)malloc(sizeof(State_t));
    state->saved_stdout = -1;
    return state;
}

///
/// Print shell prompt
///
void print_prompt(State_t *state) {
    if (state->saved_stdout == -1) {
        printf("db > ");
    }
}

///
/// Print the user in the specific format
///
void print_user(User_t *user, SelectArgs_t *sel_args) {
    size_t idx;
    printf("(");
    for (idx = 0; idx < sel_args->fields_len; idx++) {
        if (!strncmp(sel_args->fields[idx], "*", 1)) {
            printf("%d, %s, %s, %d", user->id, user->name, user->email, user->age);
        } else {
            if (idx > 0) printf(", ");

            if (!strncmp(sel_args->fields[idx], "id", 2)) {
                printf("%d", user->id);
            } else if (!strncmp(sel_args->fields[idx], "name", 4)) {
                printf("%s", user->name);
            } else if (!strncmp(sel_args->fields[idx], "email", 5)) {
                printf("%s", user->email);
            } else if (!strncmp(sel_args->fields[idx], "age", 3)) {
                printf("%d", user->age);
            }
        }
    }
    printf(")\n");
}

///
/// Print the users for given offset and limit restriction
///
void print_users(Table_t *table, int *idxList, size_t idxListLen, Command_t *cmd) {
    size_t idx;
    int limit = cmd->cmd_args.sel_args.limit;
    int offset = cmd->cmd_args.sel_args.offset;

    if (offset == -1) {
        offset = 0;
    }

    if (idxList) {
        for (idx = offset; idx < idxListLen; idx++) {
            if (limit != -1 && (idx - offset) >= limit) {
                break;
            }
            print_user(get_User(table, idxList[idx]), &(cmd->cmd_args.sel_args));
        }
    } else {
        for (idx = offset; idx < table->len; idx++) {
            if (limit != -1 && (idx - offset) >= limit) {
                break;
            }
            print_user(get_User(table, idx), &(cmd->cmd_args.sel_args));
        }
    }
}

///
/// This function received an output argument
/// Return: category of the command
///
int parse_input(char *input, Command_t *cmd) {
    char *token;
    int idx;
    token = strtok(input, " ,\n");
    for (idx = 0; strlen(cmd_list[idx].name) != 0; idx++) {
        if (!strncmp(token, cmd_list[idx].name, cmd_list[idx].len)) {
            cmd->type = cmd_list[idx].type;
        }
    }
    while (token != NULL) {
        add_Arg(cmd, token);
        token = strtok(NULL, " ,\n");
    }
    return cmd->type;
}

///
/// Handle built-in commands
/// Return: command type
///
void handle_builtin_cmd(Table_t *table, Command_t *cmd, State_t *state) {
    if (!strncmp(cmd->args[0], ".exit", 5)) {
        archive_table(table);
        exit(0);
    } else if (!strncmp(cmd->args[0], ".output", 7)) {
        if (cmd->args_len == 2) {
            if (!strncmp(cmd->args[1], "stdout", 6)) {
                close(1);
                dup2(state->saved_stdout, 1);
                state->saved_stdout = -1;
            } else if (state->saved_stdout == -1) {
                int fd = creat(cmd->args[1], 0644);
                state->saved_stdout = dup(1);
                if (dup2(fd, 1) == -1) {
                    state->saved_stdout = -1;
                }
                __fpurge(stdout); //This is used to clear the stdout buffer
            }
        }
    } else if (!strncmp(cmd->args[0], ".load", 5)) {
        if (cmd->args_len == 2) {
            load_table(table, cmd->args[1]);
        }
    } else if (!strncmp(cmd->args[0], ".help", 5)) {
        print_help_msg();
    }
}

///
/// Handle query type commands
/// Return: command type
///
int handle_query_cmd(Table_t *table, Table_like_t *like_table, Command_t *cmd) {
    if (!strncmp(cmd->args[0], "insert", 6)) {
        handle_insert_cmd(table, like_table, cmd);
        return INSERT_CMD;
    } else if (!strncmp(cmd->args[0], "select", 6)) {
        unsigned int i;
        int table_type = TABLE;
        for(i = 1 ; i < cmd->args_len; i++){
            if(!strncmp(cmd->args[i], "from", 4)){
                if(!strncmp(cmd->args[i+1], "user", 4)){
                    table_type = USER;
                    break;
                }
                else if(!strncmp(cmd->args[i+1], "like", 4)){
                    table_type = LIKE;
                    break;
                }
            }
        }
        /*default handle the table as user */
        if(table_type == USER)
            handle_user_select_cmd(table, cmd, i+2);
        else if(table_type == LIKE)
        /*unchange the table type */
            handle_like_select_cmd(like_table, cmd, i+2);
        else
            handle_user_select_cmd(table, cmd, i+2);
        return SELECT_CMD;
    } else if (!strncmp(cmd->args[0], "update", 6)) {
        handle_update_cmd(table, cmd);
        return SELECT_CMD;
    } else if (!strncmp(cmd->args[0], "delete", 6)) {
        handle_delete_cmd(table, cmd);
        return SELECT_CMD;
    } else {
        return UNRECOG_CMD;
    }
}

///
/// The return value is the number of rows insert into table
/// If the insert operation success, then change the input arg
/// `cmd->type` to INSERT_CMD
///
int handle_insert_cmd(Table_t *table,Table_like_t *like_table , Command_t *cmd) {
    int ret = 0;
    if(!strncmp(cmd->args[2], "like", 4)){
        Like_t *like = command_to_Like(cmd);
        if (like) {
            ret = add_Like(like_table, like);
            if (ret > 0) {
                cmd->type = INSERT_CMD;
            }
        }
        return ret;
    }
    else if(!strncmp(cmd->args[2], "user", 4)){
        User_t *user = command_to_User(cmd);
        if (user) {
            ret = add_User(table, user);
            if (ret > 0) {
                cmd->type = INSERT_CMD;
            }
        }
        return ret;
    }
    return ret;
}

///
/// The return value is the number of rows select from table
/// If the select operation success, then change the input arg
/// `cmd->type` to SELECT_CMD
/// This one is to handle the user table
int handle_user_select_cmd(Table_t *table, Command_t *cmd, unsigned int cmd_i) {

    field_state_handler(cmd, 1);

    /*aggregation*/
    double sum = 0, count = 0;
    int aggr_field = 0 , aggr_count = 0 , aggr_flag = 0;

    /*where correlative flag*/
    int where_flag = 0 , and_flag = 0 , or_flag = 0;
    unsigned int i = 0 ;
    where_conditon_t condition[2];

    /*table correlative parameter*/
    size_t idx , list_len = 0;
    int list[table->len];

    /*where and join command*/
    for( i = cmd_i ; i<cmd->args_len ; i++){
        if(!strncmp(cmd->args[i], "join", 4) && i+1 != cmd->args_len){
            printf("For join command\n");
            return 1;
        }
        else if(!strncmp(cmd->args[i], "where", 5) && i+1 != cmd->args_len){
            where_flag = 1;
            condition[0] = handle_where_cmd(cmd,i+1);
            i = condition[0].com_cur_len;
        }
        else if(!strncmp(cmd->args[i], "and", 3) && i+1 != cmd->args_len){
            and_flag = 1;
            condition[1] = handle_where_cmd(cmd,i+1);
            i = condition[1].com_cur_len;
        }
        else if(!strncmp(cmd->args[i], "or", 2) && i+1 != cmd->args_len){
            or_flag = 1;
            condition[1] = handle_where_cmd(cmd,i+1);
            i = condition[1].com_cur_len;
        }
    }
    /*select table by where condition*/
    for (idx = 0; idx < table->len; idx++) {
        User_t *user = get_User(table, idx);
        int print_flag = 0;
        /*one where condition*/
        if(where_flag && !and_flag && !or_flag){
            print_flag = check_where_condition(user,condition[0]);
        }
        /*two where condtion and*/
        else if(where_flag && and_flag){
            if(check_where_condition(user,condition[0]) && check_where_condition(user,condition[1]))
                print_flag = 1;
        }
        /*two where condition or*/
        else if(where_flag && or_flag){
            if(check_where_condition(user,condition[0]) || check_where_condition(user,condition[1]))
                print_flag = 1;
        }
        /*no where condition*/
        else{
            print_flag = 1;
        }
        /*該row不須被印出 一> 直接忽略該row*/
        if(!print_flag){
            continue;
        }
        /*result tabel index list*/
        list[list_len++] = idx;
    }
    /*deal with aggregation*/
    for(i = 1 ; i < cmd->args_len ; i++){
        if(strstr(cmd->args[i] , "avg")){
            aggr_flag = AVG;
            for(unsigned int j = i ; j < cmd->args_len ; j++){
                if(strstr(cmd->args[j] , "id")){
                    aggr_field = ID;
                    break;
                }
                else if(strstr(cmd->args[j] , "name")){
                    aggr_field = NAME;
                    break;
                }
                else if(strstr(cmd->args[j] , "email")){
                    aggr_field = EMAIL;
                    break;
                }
                else if(strstr(cmd->args[j] , "age")){
                    aggr_field = AGE;
                    break;
                }
            }
        }
        else if(strstr(cmd->args[i] , "count")){
            aggr_flag = COUNT;
            for(unsigned int j = i ; j < cmd->args_len ; j++){
                if(strstr(cmd->args[j] , "id")){
                    aggr_field = ID;
                    break;
                }
                else if(strstr(cmd->args[j] , "name")){
                    aggr_field = NAME;
                    break;
                }
                else if(strstr(cmd->args[j] , "email")){
                    aggr_field = EMAIL;
                    break;
                }
                else if(strstr(cmd->args[j] , "age")){
                    aggr_field = AGE;
                    break;
                }
            }
        }
        else if(strstr(cmd->args[i] , "sum")){
            aggr_flag = SUM;
            for(unsigned int j = i ; j < cmd->args_len ; j++){
                if(strstr(cmd->args[j] , "id")){
                    aggr_field = ID;
                    break;
                }
                else if(strstr(cmd->args[j] , "name")){
                    aggr_field = NAME;
                    break;
                }
                else if(strstr(cmd->args[j] , "email")){
                    aggr_field = EMAIL;
                    break;
                }
                else if(strstr(cmd->args[j] , "age")){
                    aggr_field = AGE;
                    break;
                }
            }
        }
        /*don't exist aggregation -> print normal table*/
        else if(!aggr_flag){
            print_users(table, list, list_len, cmd);
            break;
        }
        /*handle the limit and offset*/
        int limit = cmd->cmd_args.sel_args.limit;
        int offset = cmd->cmd_args.sel_args.offset;
        
        if(aggr_flag && ( offset >= 1 || ( limit < 1 && limit > -1 ) ) ){
            cmd->type = SELECT_CMD;
            return table->len;
        }
        /*first aggregation*/
        if(!aggr_count)
            printf("(");
        else
        {
            printf(", ");
            count = 0;
            sum = 0;
        }
        
        for( unsigned int j = 0 ; j < list_len ; j++){
            User_t *user = get_User( table , list[j]);
            /*aggregation compute*/
            if(aggr_flag){
                count++;
                switch (aggr_field)
                {
                case ID :
                    sum += (double)user->id;
                    break;
                
                case AGE :
                    sum += (double)user->age;
                    break;
                }
                continue;
            }

        }
        if(aggr_flag){
            switch (aggr_flag)
            {
            case AVG :
                if(count)
                    printf("%.3lf",sum/count);
                else
                    printf("0.000");
                break;
            case COUNT :
                printf("%d",(int)count);
                break;
            case SUM :
                printf("%d",(int)sum);
                break;
            }
        }

        aggr_count++;
        /*exist another aggregation*/
        if(strstr(cmd->args[i+1] , "avg") != NULL || strstr(cmd->args[i+1] , "count") != NULL || strstr(cmd->args[i+1] , "sum") != NULL){
        }
        /*can't find any other aggregation end the loop*/
        else if(aggr_flag){
            printf(")\n");
            break;
        }
    }

    cmd->type = SELECT_CMD;
    return table->len;
}

/// This one is to handle the like table
/// unchange the table
int handle_like_select_cmd(Table_like_t *like_table, Command_t *cmd, unsigned int cmd_i) {

    field_state_handler(cmd, 1);

    /*aggregation*/
    double sum = 0, count = 0;
    int aggr_field = 0 , aggr_count = 0 , aggr_flag = 0;

    // /*where correlative flag*/
    // int where_flag = 0 , and_flag = 0 , or_flag = 0;
    unsigned int i = 0 ;
    // where_conditon_t condition[2];

    // /*table correlative parameter*/
    // size_t idx , list_len = 0;
    // int list[table->len];

    // /*where and join command*/
    // for( i = cmd_i ; i<cmd->args_len ; i++){
    //     if(!strncmp(cmd->args[i], "join", 4) && i+1 != cmd->args_len){
    //         printf("For join command\n");
    //         return 1;
    //     }
    //     else if(!strncmp(cmd->args[i], "where", 5) && i+1 != cmd->args_len){
    //         where_flag = 1;
    //         condition[0] = handle_where_cmd(cmd,i+1);
    //         i = condition[0].com_cur_len;
    //     }
    //     else if(!strncmp(cmd->args[i], "and", 3) && i+1 != cmd->args_len){
    //         and_flag = 1;
    //         condition[1] = handle_where_cmd(cmd,i+1);
    //         i = condition[1].com_cur_len;
    //     }
    //     else if(!strncmp(cmd->args[i], "or", 2) && i+1 != cmd->args_len){
    //         or_flag = 1;
    //         condition[1] = handle_where_cmd(cmd,i+1);
    //         i = condition[1].com_cur_len;
    //     }
    // }
    // /*select table by where condition*/
    // for (idx = 0; idx < table->len; idx++) {
    //     User_t *user = get_User(table, idx);
    //     int print_flag = 0;
    //     /*one where condition*/
    //     if(where_flag && !and_flag && !or_flag){
    //         print_flag = check_where_condition(user,condition[0]);
    //     }
    //     /*two where condtion and*/
    //     else if(where_flag && and_flag){
    //         if(check_where_condition(user,condition[0]) && check_where_condition(user,condition[1]))
    //             print_flag = 1;
    //     }
    //     /*two where condition or*/
    //     else if(where_flag && or_flag){
    //         if(check_where_condition(user,condition[0]) || check_where_condition(user,condition[1]))
    //             print_flag = 1;
    //     }
    //     /*no where condition*/
    //     else{
    //         print_flag = 1;
    //     }
    //     /*該row不須被印出 一> 直接忽略該row*/
    //     if(!print_flag){
    //         continue;
    //     }
    //     /*result tabel index list*/
    //     list[list_len++] = idx;
    // }
    /*deal with aggregation*/
    for(i = 1 ; i < cmd->args_len ; i++){
        if(strstr(cmd->args[i] , "avg")){
            aggr_flag = AVG;
            for(unsigned int j = i ; j < cmd->args_len ; j++){
                if(strstr(cmd->args[j] , "id1")){
                    aggr_field = ID1;
                    break;
                }
                else if(strstr(cmd->args[j] , "id2")){
                    aggr_field = ID2;
                    break;
                }
            }
        }
        else if(strstr(cmd->args[i] , "count")){
            aggr_flag = COUNT;
            for(unsigned int j = i ; j < cmd->args_len ; j++){
                if(strstr(cmd->args[j] , "id1")){
                    aggr_field = ID1;
                    break;
                }
                else if(strstr(cmd->args[j] , "id2")){
                    aggr_field = ID2;
                    break;
                }
            }
        }
        else if(strstr(cmd->args[i] , "sum")){
            aggr_flag = SUM;
            for(unsigned int j = i ; j < cmd->args_len ; j++){
                if(strstr(cmd->args[j] , "id1")){
                    aggr_field = ID1;
                    break;
                }
                else if(strstr(cmd->args[j] , "id2")){
                    aggr_field = ID2;
                    break;
                }
            }
        }
        /*don't exist aggregation -> print normal table*/
        else if(!aggr_flag){
            /*needing to insert the function to print like table */
            for(unsigned int l = 0 ; l < like_table->len ; l++){
                Like_t *like = get_Like(like_table,l);
                printf("(%u,%u)\n",like->id1,like->id2);
            }
            break;
        }
        /*handle the limit and offset*/
        int limit = cmd->cmd_args.sel_args.limit;
        int offset = cmd->cmd_args.sel_args.offset;
        
        if(aggr_flag && ( offset >= 1 || ( limit < 1 && limit > -1 ) ) ){
            cmd->type = SELECT_CMD;
            return like_table->len;
        }
        /*first aggregation*/
        if(!aggr_count)
            printf("(");
        else
        {
            printf(", ");
            count = 0;
            sum = 0;
        }
        
        for( unsigned int j = 0 ; j < like_table->len ; j++){
            Like_t *like = get_Like(like_table,j);
            /*aggregation compute*/
            if(aggr_flag){
                count++;
                switch (aggr_field)
                {
                case ID1 :
                    sum += (double)like->id1;
                    break;
                
                case ID2 :
                    sum += (double)like->id2;
                    break;
                }
                continue;
            }
        }
        if(aggr_flag){
            switch (aggr_flag)
            {
            case AVG :
                if(count)
                    printf("%.3lf",sum/count);
                else
                    printf("0.000");
                break;
            case COUNT :
                printf("%d",(int)count);
                break;
            case SUM :
                printf("%d",(int)sum);
                break;
            }
        }

        aggr_count++;
        /*exist another aggregation*/
        if(strstr(cmd->args[i+1] , "avg") != NULL || strstr(cmd->args[i+1] , "count") != NULL || strstr(cmd->args[i+1] , "sum") != NULL){
        }
        /*can't find any other aggregation end the loop*/
        else if(aggr_flag){
            printf(")\n");
            break;
        }
    }

    cmd->type = SELECT_CMD;
    return like_table->len;
}

int handle_update_cmd(Table_t *table, Command_t *cmd){

    /*where correlative flag*/
    int where_flag = 0 , and_flag = 0 , or_flag = 0;
    unsigned int i = 0 ;
    where_conditon_t condition[2];

    /*update_content*/
    where_conditon_t up_cont={.field = 0 , .com_cur_len = cmd->args_len};

    char *ptr = NULL;
    /*find update content*/
    for(i = 1 ; i<cmd->args_len ; i++){
        ptr = cmd->args[i];
        if(up_cont.field == 0){
            if(strstr(cmd->args[i] , "id")){
                ptr = strstr(cmd->args[i] , "id")+2;
                up_cont.field = ID;
            }
            else if(strstr(cmd->args[i] , "name")){
                ptr = strstr(cmd->args[i] , "name")+4;
                up_cont.field = NAME;
            }
            else if(strstr(cmd->args[i] , "email")){
                ptr = strstr(cmd->args[i] , "email")+5;
                up_cont.field = EMAIL;
            }
            else if(strstr(cmd->args[i] , "age")){
                ptr = strstr(cmd->args[i] , "age")+3;
                up_cont.field = AGE;
            }
        }
        if(ptr - cmd->args[i] != strlen(cmd->args[i]) && strlen(up_cont.op) == 0){
            if(strstr(cmd->args[i] , "=")){
                ptr = strstr(cmd->args[i] , "=");
                strncpy(up_cont.op , ptr , 1);
                ptr += 1;
            }
        }
        if(strlen(up_cont.op) != 0 && ptr - cmd->args[i] != strlen(cmd->args[i]) && strlen(up_cont.content) == 0){
            strcpy(up_cont.content,ptr);
        }
        /*找到update所需的資訊後break*/
        if(up_cont.field != 0 && strlen(up_cont.op) != 0 && strlen(up_cont.content) != 0)
            break;
    }

    /*where command*/
    for(  ; i<cmd->args_len ; i++){
        if(!strncmp(cmd->args[i], "where", 5) && i+1 != cmd->args_len){
            where_flag = 1;
            condition[0] = handle_where_cmd(cmd,i+1);
            i = condition[0].com_cur_len;
        }
        else if(!strncmp(cmd->args[i], "and", 3) && i+1 != cmd->args_len){
            and_flag = 1;
            condition[1] = handle_where_cmd(cmd,i+1);
            i = condition[1].com_cur_len;
        }
        else if(!strncmp(cmd->args[i], "or", 2) && i+1 != cmd->args_len){
            or_flag = 1;
            condition[1] = handle_where_cmd(cmd,i+1);
            i = condition[1].com_cur_len;
        }
    }
    /*select table by where condition and update table*/
    for (i = 0; i < table->len; i++) {

        /*check primary key*/
        if(up_cont.field == ID){
            int primary_check = 0;
            for(unsigned int j = 0 ; j < table->len ; j++){
                User_t *temp = get_User(table,j);
                if(temp->id == atoi(up_cont.content)){
                    primary_check = 1;
                break;
                }
            }
            if(primary_check)
                break;
        }
        
        User_t *user = get_User(table, i);
        int print_flag = 0;
        /*one where condition*/
        if(where_flag && !and_flag && !or_flag){
            print_flag = check_where_condition(user,condition[0]);
        }
        /*two where condtion and*/
        else if(where_flag && and_flag){
            if(check_where_condition(user,condition[0]) && check_where_condition(user,condition[1]))
                print_flag = 1;
        }
        /*two where condition or*/
        else if(where_flag && or_flag){
            if(check_where_condition(user,condition[0]) || check_where_condition(user,condition[1]))
                print_flag = 1;
        }
        /*no where condition*/
        else{
            print_flag = 1;
        }
        /*該row不須被跟新 一> 直接忽略該row*/
        if(!print_flag){
            continue;
        }

        /*update*/
        switch (up_cont.field)
        {
        case ID : 
            user->id = atoi(up_cont.content);
            break;
        case NAME : 
            memset(user->name,'\0',sizeof(user->name));
            strcpy(user->name,up_cont.content);
            break;
        case EMAIL : 
            memset(user->email,'\0',sizeof(user->email));
            strcpy(user->email,up_cont.content);
            break;
        case AGE : 
            user->age =  atoi(up_cont.content);
            break;
        }
    }

    return table->len;
}

int handle_delete_cmd(Table_t *table, Command_t *cmd){
    /*where correlative flag*/
    int where_flag = 0 , and_flag = 0 , or_flag = 0;
    unsigned int i = 0 ;
    where_conditon_t condition[2];

    /*table correlative parameter*/
    size_t idx , list_len = 0;
    int list[table->len];

    /*where command*/
    for( i = 1 ; i<cmd->args_len ; i++){
        if(!strncmp(cmd->args[i], "where", 5) && i+1 != cmd->args_len){
            where_flag = 1;
            condition[0] = handle_where_cmd(cmd,i+1);
            i = condition[0].com_cur_len;
        }
        else if(!strncmp(cmd->args[i], "and", 3) && i+1 != cmd->args_len){
            and_flag = 1;
            condition[1] = handle_where_cmd(cmd,i+1);
            i = condition[1].com_cur_len;
        }
        else if(!strncmp(cmd->args[i], "or", 2) && i+1 != cmd->args_len){
            or_flag = 1;
            condition[1] = handle_where_cmd(cmd,i+1);
            i = condition[1].com_cur_len;
        }
    }
    /*select table by where condition*/
    for (idx = 0; idx < table->len; idx++) {
        User_t *user = get_User(table, idx);
        int delete_flag = 0;
        /*one where condition*/
        if(where_flag && !and_flag && !or_flag){
            delete_flag = check_where_condition(user,condition[0]);
        }
        /*two where condtion and*/
        else if(where_flag && and_flag){
            if(check_where_condition(user,condition[0]) && check_where_condition(user,condition[1]))
                delete_flag = 1;
        }
        /*two where condition or*/
        else if(where_flag && or_flag){
            if(check_where_condition(user,condition[0]) || check_where_condition(user,condition[1]))
                delete_flag = 1;
        }
        /*no where condition*/
        else{
            delete_flag = 1;
        }
        /*該row不須被刪除 一> 直接忽略該row*/
        if(!delete_flag){
            continue;
        }
        /*result tabel index list*/
        list[list_len++] = idx;
    }

    int current_list_num = 0;
    size_t temp_len = 0;
    User_t *temp = (User_t*)malloc(sizeof(User_t) * INIT_TABLE_SIZE);
    for(idx = 0 ; idx < table->len ; idx++){
        if(current_list_num != list_len && list[current_list_num] == idx){
            current_list_num++;
        }
        else{
            temp[temp_len++] = table->users[idx]; 
        }
    }
    free(table->users);
    table->users = temp;
    table->len = temp_len;

    return table->len;
}

/*len 為接在where後第一個argument*/
where_conditon_t handle_where_cmd(Command_t *cmd,size_t len){
    unsigned int i;
    where_conditon_t condition={.field = 0 , .com_cur_len = len};
    char *ptr = NULL;
    for(i=len ; i<cmd->args_len ; i++){
        ptr = cmd->args[i];
        if(condition.field == 0){
            if(strstr(cmd->args[i] , "id")){
                ptr = strstr(cmd->args[i] , "id")+2;
                condition.field = ID;
            }
            else if(strstr(cmd->args[i] , "name")){
                ptr = strstr(cmd->args[i] , "name")+4;
                condition.field = NAME;
            }
            else if(strstr(cmd->args[i] , "email")){
                ptr = strstr(cmd->args[i] , "email")+5;
                condition.field = EMAIL;
            }
            else if(strstr(cmd->args[i] , "age")){
                ptr = strstr(cmd->args[i] , "age")+3;
                condition.field = AGE;
            }
        }
        if(ptr - cmd->args[i] != strlen(cmd->args[i]) && strlen(condition.op) == 0){
            if(strstr(cmd->args[i] , ">=")){
                ptr = strstr(cmd->args[i] , ">=");
                strncpy(condition.op , ptr , 2);
                ptr += 2;
            }
            else if(strstr(cmd->args[i] , "<=")){
                ptr = strstr(cmd->args[i] , "<=");
                strncpy(condition.op , ptr , 2);
                ptr += 2;
            }
            else if(strstr(cmd->args[i] , "!=")){
                ptr = strstr(cmd->args[i] , "!=");
                strncpy(condition.op , ptr , 2);
                ptr += 2;
            }
            else if(strstr(cmd->args[i] , ">")){
                ptr = strstr(cmd->args[i] , ">");
                strncpy(condition.op , ptr , 1);
                ptr += 1;
            }
            else if(strstr(cmd->args[i] , "<")){
                ptr = strstr(cmd->args[i] , "<");
                strncpy(condition.op , ptr , 1);
                ptr += 1;
            }
            else if(strstr(cmd->args[i] , "=")){
                ptr = strstr(cmd->args[i] , "=");
                strncpy(condition.op , ptr , 1);
                ptr += 1;
            }
        }
        if(strlen(condition.op) != 0 && ptr - cmd->args[i] != strlen(cmd->args[i]) && strlen(condition.content) == 0){
            strcpy(condition.content,ptr);
        }
        /*check condition content*/
        // if(ptr){
        //     printf("field : %d\n",condition.field);
        //     printf("op : %s\n",condition.op);
        //     printf("content : %s\n",condition.content);
        // }
        /*找到where所需的資訊後break*/
        if(condition.field != 0 && strlen(condition.op) != 0 && strlen(condition.content) != 0)
            break;
    }
    condition.com_cur_len = i;
    /*回傳下一個argument在array中的位置*/
    return condition;
}

int check_where_condition(User_t *user,where_conditon_t condition){
    double num = atof(condition.content);
    switch (condition.field)
            {
            case ID:
                if(!strcmp(condition.op,"=")){
                    if(user->id == num)
                        return 1;
                }
                else if(!strcmp(condition.op,"!=")){
                    if(user->id != num)
                        return 1;
                }
                else if(!strcmp(condition.op,">=")){
                    if(user->id >= num)
                        return 1;
                }
                else if(!strcmp(condition.op,"<=")){
                    if(user->id <= num)
                        return 1;
                }
                else if(!strcmp(condition.op,">")){
                    if(user->id > num)
                        return 1;
                }
                else if(!strcmp(condition.op,"<")){
                    if(user->id < num)
                        return 1;
                }
                break;
            
            case NAME:
                if(!strcmp(condition.op,"=")){
                    if(!strcmp( condition.content , user->name ))
                        return 1;
                }
                else if(!strcmp(condition.op,"!=")){
                    if(strcmp( condition.content , user->name ))
                        return 1;
                }
                break;

            case EMAIL:
                if(!strcmp(condition.op,"=")){
                    if(!strcmp( condition.content , user->email ))
                        return 1;
                }
                else if(!strcmp(condition.op,"!=")){
                    if(strcmp( condition.content , user->email ))
                        return 1;
                }
                break;

            case AGE:
                if(!strcmp(condition.op,"=")){
                    if(user->age == num)
                        return 1;
                }
                else if(!strcmp(condition.op,"!=")){
                    if(user->age != num)
                        return 1;
                }
                else if(!strcmp(condition.op,">=")){
                    if(user->age >= num)
                        return 1;
                }
                else if(!strcmp(condition.op,"<=")){
                    if(user->age <= num)
                        return 1;
                }
                else if(!strcmp(condition.op,">")){
                    if(user->age > num)
                        return 1;
                }
                else if(!strcmp(condition.op,"<")){
                    if(user->age < num)
                        return 1;
                }
                break;
            }
        return 0 ;
}

///
/// Show the help messages
///
void print_help_msg() {
    const char msg[] = "# Supported Commands\n"
    "\n"
    "## Built-in Commands\n"
    "\n"
    "  * .exit\n"
    "\tThis cmd archives the table, if the db file is specified, then exit.\n"
    "\n"
    "  * .output\n"
    "\tThis cmd change the output strategy, default is stdout.\n"
    "\n"
    "\tUsage:\n"
    "\t    .output (<file>|stdout)\n\n"
    "\tThe results will be redirected to <file> if specified, otherwise they will display to stdout.\n"
    "\n"
    "  * .load\n"
    "\tThis command loads records stored in <DB file>.\n"
    "\n"
    "\t*** Warning: This command will overwrite the records already stored in current table. ***\n"
    "\n"
    "\tUsage:\n"
    "\t    .load <DB file>\n\n"
    "\n"
    "  * .help\n"
    "\tThis cmd displays the help messages.\n"
    "\n"
    "## Query Commands\n"
    "\n"
    "  * insert\n"
    "\tThis cmd inserts one user record into table.\n"
    "\n"
    "\tUsage:\n"
    "\t    insert <id> <name> <email> <age>\n"
    "\n"
    "\t** Notice: The <name> & <email> are string without any whitespace character, and maximum length of them is 255. **\n"
    "\n"
    "  * select\n"
    "\tThis cmd will display all user records in the table.\n"
    "\n";
    printf("%s", msg);
}

