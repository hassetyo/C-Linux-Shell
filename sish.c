#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h> 
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_SIZE 1000
#define HISTORY_SIZE 100

// Pipe commands
void pipecmd(char *myargs[], int amount) {
    int pipefd[2];  //pipes file discriptors
    int lastfd = STDIN_FILENO;  //contains the last commands file discriptor
    char *savptr;

    //Loop for every command
    for (int i = 0; i < amount; i++) {
        //holds pid
        pid_t cpid;
        
        // error handling
        if (pipe(pipefd) == -1) { 
            perror("pipe creation failure");
            exit(EXIT_FAILURE);
        }
        
        //forks with error handling
        if ((cpid = fork()) == -1) { 
            perror("fork failure");
            exit(EXIT_FAILURE);
        } 
        else if (cpid == 0) { 
             //child process
             
             //if it is not the first command redirect input
            if (lastfd != STDIN_FILENO) {
                if (dup2(lastfd, STDIN_FILENO) == -1) { 
                    perror("dup2 stdin failure");
                    exit(EXIT_FAILURE);
                }
                close(lastfd); 
            }
            
            // if it is not the last command redirect output
            if (i < amount - 1) {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) { 
                    perror("dup2 stdout failure");
                    exit(EXIT_FAILURE);
                }
            }
            
            //closes read and write ends of pipe
            close(pipefd[0]);
            close(pipefd[1]);
            
            //tokenizes the command
            char *args[MAX_SIZE]; 
            char *token = strtok_r(myargs[i], " \n", &savptr); 
            int count = 0;

            while (token != NULL) {
                args[count] = token; // adds to args
                count++; //increments index
                token = strtok_r(NULL, " \n", &savptr);
            }
            args[count] = NULL; 
            
            //executes command
            execvp(args[0], args); 
            
            //perror
            perror("execvp failure");
            exit(EXIT_FAILURE);
        } 
        else { 
            //parent process
            
            //closes the write end of pipe
            close(pipefd[1]);
            
            //if it is not the first command, close input
            if (lastfd != STDIN_FILENO) {
                close(lastfd);
            }
            
            //next commands input reads from pipe
            lastfd = pipefd[0]; 
        }
    }

    //waits
    for (int i = 0; i < amount; i++) {
        wait(NULL);
    }
}

//Executes command
void command(char *myargs[]) {
    //forks
    pid_t rc = fork();
    
    // error handling
        if(rc == -1) {
            perror("fork failure");
            exit(EXIT_FAILURE);
        }
        else if(rc == 0) {
            //Child executes command with execvp
            execvp(myargs[0], myargs);
            perror("execvp failure");
            exit(EXIT_FAILURE);
        }
        else {
            //parent waits
            wait(NULL);
        }
}

//Prints the history 
void printHistory(char *history[]) {
    int i = 0;
    
    //Prints all the commands in history in order from 0 - 99, with 0 being the oldest command
    while (history[i] != NULL && i < 100) {
        printf("%i.) %s", i, history[i]);
        i++;
    }
}

//Adds input to history
void addHistory(char *command, char *history[]) {
    int i = 0;
    
    //determines where it should go in history
    while (history[i] != NULL && i < 100) {
        i++;
    }
    
    //if it is the 100th command shift every commands down 1, and add command to the last spot
    if (i == 99) {
        for (int j = 0; j < 99; j++) {
            history[j] = history[j + 1];
        }
        history[99] = strdup(command);
    } else{
        //if now the 100th command add it to the array
        history[i] = strdup(command);
    }
}

//Check if the user command is a built in command
void commandcheck(char *myargs[], char *history[], char *token) {
    
    //if command is exit it terminates
    if(strcmp(myargs[0], "exit") == 0) {
            exit(EXIT_SUCCESS);
    } //if command is cd
    else if(strcmp(myargs[0], "cd") == 0) {
            if(myargs[1] == NULL) {
                printf("no directory\n");
            }
            else {
                chdir(myargs[1]); // changes directorys with chdir()
            }
    } // if command is history
    else if (strcmp(myargs[0], "history") == 0) {
            // checks for a clear history command
            if(myargs[1] != NULL && strcmp(myargs[1], "-c") == 0) { 
                for(int i = 0; history[i] != NULL; i++) {
                    //Makes history all NULL
                    history[i] = NULL; 
                }
                
            } //if history with an offset
            else if(myargs[1] != NULL) {
                
                //gets the offset
                int offset;
                sscanf(myargs[1], "%d", &offset);
                myargs[1] = NULL;
            
                if((offset >= 0 || offset < 100) && history[offset] != NULL) {
                    
                    //gets length of the command
                    int length = strlen(history[offset]);
                    
                    //cmd now hilds the command
                    char *cmd = strdup(history[offset]);
                    
                    //boolean to determine if command has a pipe
                    _Bool hasPipe = false;
                    int amount = 0;
                    for (int j = 0; j < length; j++) {
                        if (cmd[j] == '|') {
                            hasPipe = true;
                            amount++;
                        }
                    }
                    //if it doesnt have a pipe
                    if (!hasPipe) {
                        
                        //tokenizes it
                        token = strtok(cmd, " \n");
                        int i = 0;
                        for(;token != NULL && i < 99; i++) {
                            myargs[i] = token; // adds to myargs
                            token = strtok(NULL, " \n");
                        }
                        myargs[i] = NULL;
                        //sends command to command check to check if command in history [offset] is buil-in otherwise go to command function
                        commandcheck(myargs, history, token); 
                        free(cmd); 
                        
                    } 
                    else {
                        //if it does have a pipe
                        int num = 0;
                        
                        //tokenizes it
                        token = strtok(cmd, "|");
                        while (token != NULL) {
                            myargs[num] = token; // adds to myargs
                            num++;
                            token = strtok(NULL, "|");
                        }
                        pipecmd(myargs, amount + 1); // sends to pipe command
                    }
                }
            }
            else if(myargs[1] == NULL){
                //if argument is just history print out history
                printHistory(history);
            }
    }
    else {
        //if not a built in command go to command function
        command(myargs);  
    }
        
}

int main() {
        //Initializing variables
    char in[MAX_SIZE];
    char *input = in;
    char *token;
    char *token2;
    char *myargs[MAX_SIZE];
    char *history[HISTORY_SIZE];
    size_t len = MAX_SIZE;
    char *savptr;
    
    //Whiel loop for input and tokenizing input
    while(1) {
        //Creating booleans to determine if command is pipe
        _Bool isPipe = false;
        
        
        int amount = 0; //To determine how many commands there are for pipes
        
        printf("sish> ");
        
        //Gets users input and adds it to history
        getline(&input, &len, stdin); 
        addHistory(input, history);
        
        //Gets the length of command
        int cmdLen = strlen(input);
        
        //Determines how many pipe commands there are
        for (int j = 0; j < cmdLen; j++) {
            if (input[j] == '|') {
                isPipe = true;
                amount++;
            }
        }
        
        //If the command doesnt use any pipes
        if (!isPipe) {
            
            //tokenizes it
            token = strtok_r(input, " \n", &savptr);
            int i;
            for(i = 0; token != NULL && i < 99; i++) {
                myargs[i] = token; //Puts command in myargs
                token = strtok_r(NULL, " \n", &savptr);
            }
            myargs[i] = NULL;
            commandcheck(myargs, history, token); //Sends commands to command check
        } 
        else {
            //If there is pipes
            int num = 0;
            //Tokenizes it
            token2 = strtok_r(input, "|", &savptr);
            while (token2 != NULL) {
                myargs[num] = token2; //Puts commands into my args
                num++;
                token2 = strtok_r(NULL, "|", &savptr);
            
            }
            pipecmd(myargs, amount + 1); // sends it to pipecmd function
        }                
    }
    return 0;
}

