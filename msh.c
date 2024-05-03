/*
  Name: James D Hofer
  ID: 1000199225
  Omega Compile: gcc msh.c -o msh —std=c99
*/

// The MIT License (MIT)
// 
// Copyright (c) 2016 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line
#define MAX_COMMAND_SIZE 255    // The maximum command-line size
#define MAX_NUM_ARGUMENTS 11    // Mav shell supports ten arguments
#define MAX_COMMAND_HISTORY 15  // Maximum number of commands to store in history
#define MAX_PID_HISTORY 15      // Maximum number of PIDs to store in history
#define PATH_SEPARATOR ":"      // Separator in PATH environment variable
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1


// Command and PID History Queue
// Note: if history size grows, linked list queue should be used
char *history[MAX_COMMAND_HISTORY];
pid_t pid_history[MAX_PID_HISTORY];


/**
 * Store command in history queue
 * @param history - array of command strings
 * @param cmd - command string to store
 */
void storeHistory(char *history[], char *cmd)
{
  // Shift array to left to make room for new command
  for(int i = 0; i < MAX_COMMAND_HISTORY-1; i++)
  {
    history[i] = history[i+1];
  }

  // Store new command in history
  history[MAX_COMMAND_HISTORY-1] = strdup(cmd);
}


/**
 * Store PID in PID history queue
 * @param pid_history - array of PIDs
 * @param pid - PID to store
 */
void storePIDHistory(pid_t pid_history[], pid_t pid)
{
  // Shift array to left to make room for new PID
  for(int i = 0; i < MAX_PID_HISTORY-1; i++)
  {
    pid_history[i] = pid_history[i+1];
  }

  // Store new PID in history
  pid_history[MAX_PID_HISTORY-1] = pid;
}


/**
 * Util func to count the number of NULLs in history queue
 * @param history - array of command strings
 * @return int - number of NULLs in history queue
 */
int countNulls(char *history[])
{
  int null_counter = 0;
  
  for(int i = 0; i < MAX_COMMAND_HISTORY; i++)
  {
    if(history[i] == NULL)
    {
      null_counter++;
    }
  }

  return null_counter;
}


/**
 * Print command history
 * @param history - array of command strings
 */
void printHistory(char *history[])
{
  int nulls = countNulls(history);
  
  // Print history array, oldest to newest
  for(int i = 0; i < MAX_COMMAND_HISTORY; i++)
  {
    if(history[i] != NULL)
    {
      printf("%d: %s", i-nulls, history[i]);
    }
  }
}


/**
 * Print PID history
 * @param pid_history - array of PIDs
 */
void printPIDHistory(pid_t pid_history[])
{
  int null_counter = 0;
  
  // Print PID history array, oldest to newest
  for(int i = 0; i < MAX_PID_HISTORY; i++)
  {
    if(pid_history[i] == 0)
    {
      null_counter++;
      continue;
    }

    printf("%d: %d\n", i-null_counter, pid_history[i]);
  }
}


/**
 * Free memory allocated for history queue
 * @param history - array of command strings
 */
void cleanupHistory(char *history[])
{
  for(int i = 0; i < MAX_COMMAND_HISTORY; i++)
  {
    free(history[i]);
  }
}


/**
 * Main function
 * @return int - exit status
 */
int main()
{

  char *command_string = (char*) malloc(MAX_COMMAND_SIZE);

  while(true)
  {
    // Print out the msh prompt
    printf ("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while(!fgets(command_string, MAX_COMMAND_SIZE, stdin));

    // Store command in history
    // emulating Linux history which stores all entries
    storeHistory(history, command_string);

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr;                                         
                                                           
    char *working_string = strdup(command_string);                

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while(((argument_ptr = strsep(&working_string, WHITESPACE)) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
      
      if(strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      
      token_count++;
    }

    
    /*** SPECIAL CASES ***/

    // Rename command var
    char *cmd = token[0];

    // "!n" Command - custom command to execute command from history
    // if invalid, continue to next iteration of loop
    if(cmd[0] == '!')
    {
      int index = atoi(cmd+1);
      int nulls = countNulls(history);

      if(index < 0 || index > MAX_COMMAND_HISTORY-1 || history[index + nulls] == NULL)
      {
        printf("Command not in history.\n");
        continue;
      }

      // remove trailing newline
      char *temp = strtok(history[index + nulls], "\n");

      // Copy command from history to cmd
      strcpy(cmd, temp);

      // add trailing newline
      strcat(history[index + nulls], "\n");
    }

    // Empty Command - Guard for empty return
    // continue to next iteration of loop - print new msh prompt
    if(cmd == NULL)
    {
      continue;
    }

    // Change Directory - cd must be handled by parent process
    // continue to next iteration of loop - print new msh prompt
    if(strcmp(cmd, "cd") == 0)
    {
      chdir(token[1]);
      continue;
    }

    // History - custom command to display command history
    // continue to next iteration of loop - print new msh prompt
    if(strcmp(cmd, "history") == 0)
    {
      printHistory(history);
      continue;
    }

    // PID History - custom command to display PID history
    // continue to next iteration of loop - print new msh prompt
    if(strcmp(cmd, "pidhistory") == 0)
    {
      printPIDHistory(pid_history);
      continue;
    }

    // Quit or Exit - Handle termination of msh
    if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
    {
      cleanupHistory(history);
      exit(EXIT_SUCCESS);
    }
    
    
    /*** COMMAND SEARCH ***/
    
    // Get System Path
    char *path = getenv("PATH");
    char *path_token;
    path_token = strtok(path, PATH_SEPARATOR);

    char *cmd_path = (char*) malloc(MAX_COMMAND_SIZE);
    bool cmd_found = false;

    // Search Present Working Directory for Command
    // check for execute bit
    if(access(cmd, X_OK) == 0)
    {
      cmd_path = cmd;
      cmd_found = true;
    }

    // Search Path for Command
    else
    {
      while(path_token != NULL)
      {
        strcpy(cmd_path, path_token);
        strcat(cmd_path, "/");
        strcat(cmd_path, cmd);

        if(access(cmd_path, X_OK) == 0)
        {
          cmd_found = true;
          break;
        }

        path_token = strtok(NULL, ":");
      }
    }

    // Command not found - Display info message
    // continue to next iteration of loop - print new msh prompt
    if(!cmd_found)
    {
      printf("%s: Command not found.\n", cmd);
      continue;
    }


    /*** EXECUTE COMMAND ***/
    
    // Fork a child process to run command with arguments
    pid_t pid = fork();
    if(pid < 0)
    {
      // Check if fork failed
      printf("Fork failed.\n");
      exit(EXIT_FAILURE);
    }
    
    else if(pid == 0)
    {
      // Child process
      execv(cmd_path, token);
    }
    
    else
    {
      // Parent process
      int status;
      (void) waitpid(pid, &status, 0);
      storePIDHistory(pid_history, pid);
      fflush(NULL);
    }

    // Loop Cleanup
    free(head_ptr);
    free(cmd_path);
  }

  return EXIT_SUCCESS;
  // e2520ca2-76f3-90d6-0242ac120003
}