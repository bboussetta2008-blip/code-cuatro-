
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;

    while (1)
    {
        printf("ocsh> ");
        fflush(stdout);

        // 1. Read command
        if (getline(&line, &capacity, stdin) == -1)
        {
            printf("\n");
            break;
        }

        // 2. Split command into arguments
        char *args[MAX_ARGS];
        int i = 0;

        char *token = strtok(line, " \t\n");

        while (token != NULL && i < MAX_ARGS - 1)
        {
            args[i++] = token;
            token = strtok(NULL, " \t\n");
        }

        args[i] = NULL;

        // Ignore empty commands
        if (i == 0)
            continue;

        // 3. Builtin quit
        if (strcmp(args[0], "quit") == 0)
            break;

        // 4. Create child process
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
        }
        else if (pid == 0)
        {
            // Child process
            execvp(args[0], args);

            perror("execvp");
            _exit(127);
        }
        else
        {
            // Parent process
            waitpid(pid, NULL, 0);
        }
    }

    free(line);
    return 0;
}
