#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT 256
#define MAX_ARGS  16



void multi_pipes(char* commands[][MAX_ARGS], int num_comm)
{
    int pipefd[2 * (num_comm - 1)];

    /* create all pipes */
    for (int i = 0; i < num_comm - 1; i++)
    {
        if (pipe(pipefd + i * 2) < 0)
        {
            perror("pipe");
            return;
        }
    }

    /* create one child per command */
    for (int i = 0; i < num_comm; i++)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            return;
        }

        if (pid == 0)
        {
            /* not first command */
            if (i > 0)
            {
                if (dup2(pipefd[(i - 1) * 2],
                    STDIN_FILENO) < 0)
                {
                    perror("dup2 stdin");
                    exit(EXIT_FAILURE);
                }
            }

            /* not last command */
            if (i < num_comm - 1)
            {
                if (dup2(pipefd[i * 2 + 1],
                    STDOUT_FILENO) < 0)
                {
                    perror("dup2 stdout");
                    exit(EXIT_FAILURE);
                }
            }

            /* close all pipe descriptors */
            for (int j = 0; j < 2 * (num_comm - 1); j++)
            {
                close(pipefd[j]);
            }

            execvp(commands[i][0], commands[i]);

            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    /* parent closes all pipe descriptors */
    for (int i = 0; i < 2 * (num_comm - 1); i++)
    {
        close(pipefd[i]);
    }

    /* parent waits for all children */
    for (int i = 0; i < num_comm; i++)
    {
        wait(NULL);
    }
}

int main(void)
{
    char input[MAX_INPUT];
    char* args[MAX_ARGS];
    char* exec_args[MAX_ARGS];
    int exec_count = 0;
	int num_pipes = 0;
    int current_command = 0;
    int current_arg = 0;
	int background = 0;
    char* commands[MAX_ARGS][MAX_ARGS];
  
    while (1)
    {    
        exec_count = 0;
        num_pipes = 0;
        current_command = 0;
        current_arg = 0;
        printf("myshell> ");
        fflush(stdout);
        while (waitpid(-1, NULL, WNOHANG) > 0); // Reap any background processes
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        int argc = 0;
        char* token = strtok(input, " \t");

        while (token != NULL && argc < MAX_ARGS - 1)
        {
            args[argc++] = token;
            token = strtok(NULL, " \t");
        }

        args[argc] = NULL;

        if (argc == 0)
            continue;

        /* builtin exit */

        if (strcmp(args[0], "exit") == 0)
            break;

        /* builtin cd */

        if (strcmp(args[0], "cd") == 0)
        {
            if (args[1] == NULL)
            {
                fprintf(stderr, "cd: missing argument\n");
            }
            else
            {
                if (chdir(args[1]) != 0)
                    perror("cd");
            }

            continue;
        }

        char* infile = NULL;
        char* outfile = NULL;
        int append = 0;

   
		// redirection handling and pipeline
        for (int i = 0; args[i] != NULL; i++)
        {
            if (strcmp(args[i], "<") == 0)
            {
                if (args[i + 1] == NULL)
                {
                    fprintf(stderr, "syntax error: missing input file\n");
                    exec_count = -1;
                    break;
                }

                infile = args[i + 1];
                i++;
            }
            else if (strcmp(args[i], ">") == 0)
            {
                if (args[i + 1] == NULL)
                {
                    fprintf(stderr, "syntax error: missing output file\n");
                    exec_count = -1;
                    break;
                }

                outfile = args[i + 1];
                append = 0;
                i++;
            }
            else if (strcmp(args[i], ">>") == 0)
            {
                if (args[i + 1] == NULL)
                {
                    fprintf(stderr, "syntax error: missing output file\n");
                    exec_count = -1;
                    break;
                }

                outfile = args[i + 1];
                append = 1;
                i++;
            }
            else if (strcmp(args[i], "|") == 0)
            {
             num_pipes++;
               
			}
            else if(strcmp(args[i], "&") == 0)
            {
				background = 1;
                
				args[i] = NULL; // Terminate the command before '&'
				argc--; // Decrease argument count
			}

            else
            {
                exec_args[exec_count++] = args[i];
            }
        }
		
        if (num_pipes > 0) {

            int i = 0;
            while (args[i] != NULL) {

                if (strcmp(args[i], "|") == 0) {
                    
                    commands[current_command][current_arg] = NULL;
                    current_command++;
                    current_arg = 0;
                }
                else {
                    commands[current_command][current_arg++] = args[i];

                }
                i++;
            }
            commands[current_command][current_arg] = NULL;
            multi_pipes(commands, current_command + 1);
			continue;
        }

        if (exec_count == -1)
            continue;
        
     

        exec_args[exec_count] = NULL;

    
        
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            continue;
        }

        if (pid == 0)
        {
            /* input redirection */

            if (infile != NULL)
            {
                int fd = open(infile, O_RDONLY);

                if (fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            /* output redirection */

            if (outfile != NULL)
            {
                int fd;

                if (append)
                {
                    fd = open(outfile,
                        O_WRONLY | O_CREAT | O_APPEND,
                        0644);
                }
                else
                {
                    fd = open(outfile,
                        O_WRONLY | O_CREAT | O_TRUNC,
                        0644);
                }

                if (fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
           
            execvp(exec_args[0], exec_args);

            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else
        {
            if (!background) {
                waitpid(pid, NULL, 0);
            }

           if(background) {
                printf("Process running in background with PID: %d\n", pid);
		   }
        }
		
    }
  
    printf("Bye!\n");
    return 0;
}

   
