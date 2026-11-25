#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 100
#define MAX_HISTORY 100

static char *history[MAX_HISTORY];
static int history_count;
static int last_status;
static pid_t last_background;

static void save_history(const char *line)
{
    if (history_count == MAX_HISTORY) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (MAX_HISTORY - 1));
        history_count--;
    }
    history[history_count++] = strdup(line);
}

static void reap_children(int signal_number)
{
    (void)signal_number;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}

static void expand_word(char *word, size_t size)
{
    char result[4096];
    size_t i = 0, j = 0;

    while (word[i] && j + 1 < sizeof(result)) {
        if (word[i] == '$') {
            char name[256];
            size_t n = 0;
            const char *value = "";
            i++;
            if (word[i] == '?') {
                snprintf(name, sizeof(name), "%d", last_status);
                value = name;
                i++;
            } else if (word[i] == '!') {
                snprintf(name, sizeof(name), "%ld", (long)last_background);
                value = name;
                i++;
            } else {
                int braces = word[i] == '{';
                if (braces) i++;
                while ((word[i] == '_' || (word[i] >= 'A' && word[i] <= 'Z') ||
                        (word[i] >= 'a' && word[i] <= 'z') ||
                        (n > 0 && word[i] >= '0' && word[i] <= '9')) && n + 1 < sizeof(name))
                    name[n++] = word[i++];
                name[n] = '\0';
                if (braces && word[i] == '}') i++;
                if (n) value = getenv(name);
            }
            if (value) {
                size_t len = strlen(value);
                if (len > sizeof(result) - j - 1) len = sizeof(result) - j - 1;
                memcpy(result + j, value, len);
                j += len;
            }
        } else {
            result[j++] = word[i++];
        }
    }
    result[j] = '\0';
    snprintf(word, size, "%s", result);
}

static int tokenize(char *line, char *args[])
{
    int count = 0;
    char *p = line;
    while (*p && count < MAX_ARGS - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p || *p == '#') break;
        if (!strncmp(p, "->", 2) || !strncmp(p, ">>", 2)) {
            args[count++] = strndup(p, 2);
            p += 2;
            continue;
        }
        if (strchr("<>|&", *p)) {
            args[count++] = strndup(p, 1);
            p++;
            continue;
        }
        char *word = calloc(4096, 1);
        size_t n = 0;
        char quote = 0;
        while (*p && (quote || (!strchr(" \t\n<>|&", *p) && strncmp(p, "->", 2) && strncmp(p, ">>", 2)))) {
            if (!quote && (*p == '\'' || *p == '"')) quote = *p++;
            else if (quote && *p == quote) { quote = 0; p++; }
            else if (*p == '\\' && p[1]) { p++; word[n++] = *p++; }
            else word[n++] = *p++;
            if (n >= 4094) break;
        }
        expand_word(word, 4096);
        args[count++] = word;
    }
    args[count] = NULL;
    return count;
}

static int builtin(char *args[])
{
    if (!args[0]) return 1;
    if (!strcmp(args[0], "quit")) exit(0);
    if (!strcmp(args[0], "go")) {
        const char *dir = args[1] ? args[1] : getenv("HOME");
        if (chdir(dir) < 0) perror("go");
        return 1;
    }
    if (!strcmp(args[0], "path")) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd))) puts(cwd); else perror("path");
        return 1;
    }
    if (!strcmp(args[0], "say")) {
        int start = 1, newline = 1;
        if (args[1] && !strcmp(args[1], "-n")) { start = 2; newline = 0; }
        for (int i = start; args[i]; i++) printf("%s%s", i == start ? "" : " ", args[i]);
        if (newline) putchar('\n');
        return 1;
    }
    if (!strcmp(args[0], "info")) {
        puts("Commandes : go quit path say info past osinfo export");
        puts("Alias : list show copy move del newdir newfile");
        puts("Redirections : < > >> ; pipe : -> ; & lance en arriere-plan");
        return 1;
    }
    if (!strcmp(args[0], "osinfo")) {
        puts("OCSH - Mini-shell en C");
        puts("Projet 3 : Systemes d'exploitation");
        return 1;
    }
    if (!strcmp(args[0], "past")) {
        for (int i = 0; i < history_count; i++) printf("%d  %s", i + 1, history[i]);
        return 1;
    }
    if (!strcmp(args[0], "export")) {
        if (!args[1]) { fprintf(stderr, "Utilisation : export NOM=valeur\n"); return 1; }
        char *equal = strchr(args[1], '=');
        if (!equal) { fprintf(stderr, "Utilisation : export NOM=valeur\n"); return 1; }
        *equal = '\0';
        if (setenv(args[1], equal + 1, 1) < 0) perror("export");
        return 1;
    }
    return 0;
}

static void run_command(char *args[], int background)
{
    char *argv[MAX_ARGS];
    int argc = 0, input = -1, output = -1, append = 0;
    for (int i = 0; args[i]; i++) {
        if (!strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
            char *op = args[i];
            if (!args[i + 1]) { fprintf(stderr, "Fichier manquant apres %s\n", op); return; }
            int flags = O_RDONLY;
            if (!strcmp(op, ">") || !strcmp(op, ">>")) {
                flags = O_WRONLY | O_CREAT | (!strcmp(op, ">>") ? O_APPEND : O_TRUNC);
                append = 1;
            }
            int fd = open(args[++i], flags, 0644);
            if (fd < 0) { perror(args[i]); return; }
            if (flags == O_RDONLY) input = fd; else output = fd;
        } else argv[argc++] = args[i];
    }
    argv[argc] = NULL;
    if (argc == 0) return;
    if (!strcmp(argv[0], "list")) argv[0] = "ls";
    else if (!strcmp(argv[0], "show")) argv[0] = "cat";
    else if (!strcmp(argv[0], "copy")) argv[0] = "cp";
    else if (!strcmp(argv[0], "move")) argv[0] = "mv";
    else if (!strcmp(argv[0], "del")) argv[0] = "rm";
    else if (!strcmp(argv[0], "newdir")) argv[0] = "mkdir";
    else if (!strcmp(argv[0], "newfile")) argv[0] = "touch";

    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        if (input >= 0) { dup2(input, STDIN_FILENO); close(input); }
        if (output >= 0) { dup2(output, STDOUT_FILENO); close(output); }
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    if (input >= 0) close(input);
    if (output >= 0) close(output);
    if (pid < 0) { perror("fork"); return; }
    if (background) { last_background = pid; printf("[%ld]\n", (long)pid); }
    else {
        int status = 0;
        pid_t waited;
        do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
        if (waited == pid && WIFEXITED(status)) last_status = WEXITSTATUS(status);
        else if (waited == pid && WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
    }
    (void)append;
}

static void execute(char *line)
{
    char *args[MAX_ARGS];
    int count = tokenize(line, args);
    if (!count) return;
    int background = !strcmp(args[count - 1], "&");
    if (background) { free(args[count - 1]); args[--count] = NULL; }
    int pipe_at = -1;
    for (int i = 0; i < count; i++) if (!strcmp(args[i], "->") || !strcmp(args[i], "|")) pipe_at = i;
    if (pipe_at >= 0) {
        char *pipe_token = args[pipe_at];
        args[pipe_at] = NULL;
        char **right = &args[pipe_at + 1];
        int fds[2];
        if (!right[0] || pipe(fds) < 0) { perror("pipe"); for (int i = 0; i < count; i++) free(args[i]); return; }
        pid_t first = fork();
        if (first == 0) { dup2(fds[1], STDOUT_FILENO); close(fds[0]); close(fds[1]); run_command(args, 0); _exit(0); }
        pid_t second = fork();
        if (second == 0) { dup2(fds[0], STDIN_FILENO); close(fds[0]); close(fds[1]); run_command(right, 0); _exit(0); }
        close(fds[0]); close(fds[1]);
        if (first > 0) waitpid(first, NULL, 0);
        if (second > 0) { int s = 0; if (waitpid(second, &s, 0) == second && WIFEXITED(s)) last_status = WEXITSTATUS(s); }
        args[pipe_at] = pipe_token;
    } else {
        int has_redirect = 0;
        for (int i = 0; i < count; i++)
            if (!strcmp(args[i], ">") || !strcmp(args[i], ">>") || !strcmp(args[i], "<")) has_redirect = 1;
        if (has_redirect && args[0] &&
            (!strcmp(args[0], "go") || !strcmp(args[0], "path") || !strcmp(args[0], "say") ||
             !strcmp(args[0], "info") || !strcmp(args[0], "past") || !strcmp(args[0], "osinfo") ||
             !strcmp(args[0], "export"))) {
            int saved_in = dup(STDIN_FILENO), saved_out = dup(STDOUT_FILENO);
            char *clean[MAX_ARGS]; int n = 0;
            for (int i = 0; i < count; i++) {
                if (!strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], ">>")) {
                    char *op = args[i];
                    if (!args[i + 1]) { fprintf(stderr, "Fichier manquant apres %s\n", op); break; }
                    int flags = O_RDONLY;
                    if (strcmp(op, "<")) flags = O_WRONLY | O_CREAT | (!strcmp(op, ">>") ? O_APPEND : O_TRUNC);
                    int fd = open(args[++i], flags, 0644);
                    if (fd < 0) { perror(args[i]); break; }
                    dup2(fd, !strcmp(op, "<") ? STDIN_FILENO : STDOUT_FILENO);
                    close(fd);
                } else clean[n++] = args[i];
            }
            clean[n] = NULL;
            builtin(clean);
            fflush(stdout);
            dup2(saved_in, STDIN_FILENO); dup2(saved_out, STDOUT_FILENO);
            close(saved_in); close(saved_out);
        } else if (!builtin(args)) run_command(args, background);
    }
    for (int i = 0; i < count; i++) free(args[i]);
}

static void run_startup(void)
{
    FILE *file = fopen(".ocshrc", "r");
    if (!file) return;
    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) != -1) execute(line);
    free(line);
    fclose(file);
}

int main(int argc, char **argv)
{
    signal(SIGCHLD, reap_children);
    signal(SIGINT, SIG_IGN);
    if (argc > 1) {
        FILE *script = fopen(argv[1], "r");
        if (!script) { perror(argv[1]); return 1; }
        char *line = NULL; size_t size = 0;
        while (getline(&line, &size, script) != -1) execute(line);
        free(line); fclose(script); return last_status;
    }
    run_startup();
    char *line = NULL; size_t size = 0;
    while (1) {
        printf("ocsh> "); fflush(stdout);
        if (getline(&line, &size, stdin) == -1) { putchar('\n'); break; }
        save_history(line);
        execute(line);
    }
    free(line);
    for (int i = 0; i < history_count; i++) free(history[i]);
    return 0;
}
