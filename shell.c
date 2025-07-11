#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

char pwd[256] = "/home/victor/Desktop/shell";

typedef struct {
    char key[128];
    char value[128];
} varObj;

enum actions{
    SET,
    UNSET,
    GETVAR,
    FREE,
    ADDHISTORY,
    GETHISTORY,
    CLOSEHISTORY,
};

void str_cpy(char *, char *);
void variableAction(char *, int);  
void freeTokens(char **);


void historyAction(char *command, int mode){
    static FILE* fptr = NULL;
    if(mode == CLOSEHISTORY){
        if(fptr != NULL){
            fclose(fptr); 
            return;
        }else return;
    }
    
    if(fptr == NULL){
        fptr = fopen("history.txt", "a+");
        if(fptr ==NULL){
            perror("history open failed");
            exit(1);
        }
    }
    if(mode == ADDHISTORY){
        if(fputs(command, fptr) == EOF){
            perror("error while write to history");
            exit(1);
        }

        if(fputs("\n", fptr) == EOF){
            perror("error while write to history");
            exit(1);
        }
        
    } else if(mode == GETHISTORY){
       
        if(*command == '0') return;
        int num = atoi(command);

        if(num){
            fseek(fptr, 0, SEEK_END);
            int count = 0;
            long filesize = ftell(fptr);
            char ch;
            long pos = filesize - 1;
    
            while (pos >= 0 && count <= num) {
                fseek(fptr, pos--, SEEK_SET);
                ch = fgetc(fptr);
                if (ch == '\n') {
                    count++;
                }
            }
            fseek(fptr, pos + 2, SEEK_SET);
            char line[1024];
            while (fgets(line, sizeof(line), fptr)) {
                printf("%s", line);
            }
            
        } else{
            printf("unknown command \n");
        }

    }


}

int getFullPath(char **path){
    char newpath[256] = {};
    strcat(newpath, pwd);

    if(strncmp(*path, "..", 2)== 0){
        char *off = strrchr(newpath, '/');
        if(off !=NULL){
            char *isSingle = strchr(newpath, '/');
            printf("isSingle: %p\noff: %p\n", isSingle, off );
            if(off == isSingle){
                *(off + 1) = '\0';
            }else{
                *off = '\0';
            }
            strncat(newpath, *path+2, sizeof(newpath) - strlen(newpath) - 1);
        } else {
            printf("error while changing directory\n");
            return 1;
        }
    }
    else if(strncmp(*path, "./", 2) == 0){
        if(strcmp(newpath, "/") == 0){
        strncat(newpath, *path + 2, sizeof(newpath) - strlen(newpath) - 1);

        } else
        strncat(newpath, *path + 1, sizeof(newpath) - strlen(newpath) - 1);
    }
    else if(strncmp(*path, "/", 1) == 0){
        return 1;
    } 
    else{
        return 0;
    }

    if(strlen(newpath) > 1 && newpath[strlen(newpath)-1] == '/'){
        newpath[strlen(newpath)-1] = '\0';
    }

    char *temp = realloc(*path,strlen(newpath) +1);
    if(!temp){
        perror("realoccating err");
        exit(1);

    }
    str_cpy(temp, newpath);

    *path = temp;
    
    return 1;
}

void cd(char **path){
    struct stat sb;
    getFullPath(path);

    if(stat(*path, &sb) ==0){
        if( S_ISDIR(sb.st_mode)){
            str_cpy(pwd, *path);
        }else{
        printf("path is invalid\n");
    }
    } else{
        printf("path is invalid\n");
    }
    
}

int isBuiltIn(char **tokens,int tokenCounts){
    if (strcmp(tokens[0], "set") == 0) {
        if(tokenCounts != 2){
            printf("invalid arguments\n");
            return 1;
        }
        variableAction(tokens[1], SET);
        return 1;
    } if (strcmp(tokens[0], "pid") == 0) {
        if(tokenCounts > 2){
            printf("invalid arguments\n");
            return 1;
        }
        printf("%d", getpid());
        return 1;
    } else if (strcmp(tokens[0], "unset") == 0) {
         if(tokenCounts < 2 || tokenCounts > 3){
            printf("invalid arguments\n");
            return 1;
        }
        variableAction(tokens[1], UNSET);
        return 1;
    } else if (strcmp(tokens[0], "echo") == 0) {
         if(tokenCounts < 2){
            printf("no arguments\n");
            return 1;
        }
        int i = 1; 
        while(tokens[i] != NULL){
            printf("%s ",tokens[i]);
            i++;
        }
        printf("\n");
        return 1;
    } else if (strcmp(tokens[0], "pwd") == 0) {
        if(tokenCounts > 2){
            printf("invalid arguments\n");
            return 1;
        }
        printf("%s", pwd);
        return 1;
    } else if(strcmp(tokens[0], "cd") == 0) {
         if(tokenCounts != 2){
            printf("invalid arguments\n");
            return 1;
        }
        cd(&tokens[1]);
        return 1;
    }else if(strcmp(tokens[0], "history") == 0) {
        if(tokenCounts > 2){
            printf("invalid arguments\n");
            return 1;
        }
        historyAction((tokens[1] ? tokens[1]: "50"), GETHISTORY);
        return 1;
    }  
    return 0;
}

void tokenaizing(char *command, char **tokens, int *tokenCounts){
    freeTokens(tokens);
    int i = 0;

    char *token = strtok(command, " ");

    while(token !=NULL && i < 64){        
        tokens[i] = malloc(strlen(token) + 1);
        if(tokens[i]==NULL){
            perror("err caused by allocator");
            exit(1);
        }
        
        str_cpy(tokens[i], token);
        
        token = strtok(NULL, " ");

        i++;
    }
    tokens[i] = NULL;
    *tokenCounts = i;
}

void variableAction(char *str, int mode){
    static varObj *variables = NULL;
    static char index = 0;

    if(mode == FREE){
        if(variables != NULL){
            free(variables);
            return;
        }
        return;
    }

    if(variables == NULL){
        variables = (varObj*)malloc(sizeof(varObj) * 100);
    }
    if(mode == SET){

        char *off = strchr(str, '=');

        if(off !=NULL){

            *off = '\0';

            str_cpy(variables[index].key, str);

            str_cpy(variables[index].value, off+1);

        }else {
            printf("command set: set key=value\n");
        }
        
        index++;
        return;
    } else if(mode == GETVAR){
        for(int i = 0; i< index; i++){
            if(strcmp(variables[i].key, str+1) == 0){
                str_cpy(str, variables[i].value);
                return;
            }
        }
        str_cpy(str, " ");

    } else if(mode == UNSET){
        for(int i = 0; i< index; i++){
            if(strcmp(variables[i].key, str) == 0){
                for(int j = i; j< index-1;j++){
                    variables[i] = variables[i+1];
                }
                index--;
                break;
            }
        }
        return;
    } 

}

void str_cpy(char *dest, char *str){
     if (!dest || !str) {
        fprintf(stderr, "str_cpy: NULL arg (dest=%p, str=%p)\n", (void*)dest, (void*)str);
        return;
    }

    char *off = strchr(str, '$');

    if(off != NULL){
        if(*(off-1) != '\\'){
            variableAction(str, GETVAR);
        }
    } 
        int i = 0;
        while(str[i] != '\0'){
            dest[i] = str[i];
            i++;
        }

        dest[i] = '\0';
}

int isExternal(char **tokens, int tokenCounts){

            pid_t child = fork();
            if(child < 0) {
                perror("fork failed");
                exit(1);
            }
            if(child == 0){
                chdir(pwd);
                execvp(tokens[0], tokens);
                printf("unknown command\n");
                exit(1);
        }

        int status;

        waitpid(child,&status,0);
    return 0;

}

void cleanSpace(char *str){
    int i = 0;
    while(isspace(str[i])) i++;
    memmove(str, str + i, strlen(str+i) +1);
}

void freeTokens(char **tokens){
     for(int i = 0; tokens[i] != NULL; i++){
        free(tokens[i]);
    }
}

int main(){

    char command[256] = {};
    char **tokens = malloc(sizeof(char *) * 64);

    if(tokens == NULL){
        perror("err caused by allocator");
        exit(1);
    }

    while(1){
        int tokenCounts = 0;
        printf("mysh> ");
        if(fgets(command, sizeof(command), stdin) == NULL){
            perror("get command err");
            return 1;
        }

        cleanSpace(command);

        if(strlen(command) < 2) continue;

        command[strlen(command) - 1] = '\0';

        historyAction(command, ADDHISTORY);

        tokenaizing(command, tokens, &tokenCounts);
        if(strcmp(tokens[0], "exit") == 0) {
            break;
        }
        
        int built = isBuiltIn(tokens, tokenCounts);
        if(!built){
            isExternal(tokens, tokenCounts);
        }
    }   
    
    freeTokens(tokens);
    
    variableAction(NULL, FREE);
    historyAction(NULL,CLOSEHISTORY);
    free(tokens);
    
    return 0;
}