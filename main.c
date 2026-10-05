#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_CHAR 1024
#define MAX_INPUT_STR 30

typedef struct
{
	int pid_read;
	char state;
	unsigned long minflt, majflt, utime, stime, vsize;
	int cpuid;
} MonitorStat;

typedef struct
{
	int cap;
	int size;
	MonitorStat *data;
} StatArr;

StatArr init_arr(int cap)
{
	StatArr result;
	result.cap = cap;
	result.size = 0;
	result.data = malloc(sizeof(*result.data)*cap);

	return result;
}

void insert_arr(StatArr *arr, MonitorStat val)	
{
	if (arr->size >= arr->cap)
	{
		arr->cap *= 2;
		MonitorStat *tmp = (MonitorStat *)realloc(arr->data, sizeof(*arr->data) * arr->cap);
		if (tmp == 0)
		{
			printf("Out of memory!\n");
			return;
		}
		arr->data = tmp;
	}

	arr->data[arr->size++] = val;
}

void print_stat(StatArr arr)
{
	printf("%-5s %-6s %-8s %-8s %-17s %-8s %-8s\n",
		   "STATE", "CPUID", "UTIME", "STIME", "VSIZE", "MINFLT", "MAJFLT");
	for (int i = 0; i < arr.size; ++i)
	{
		MonitorStat stat = arr.data[i];
		printf("%-5c %-6d %-8lu %-8lu %-17lu %-8lu %-8lu\n",
			   stat.state,  
			   stat.cpuid,  
			   stat.utime,  
			   stat.stime,  
			   stat.vsize,  
			   stat.minflt, 
			   stat.majflt);
	}
}

void free_arr(StatArr *arr)
{
	if (arr->data != 0)
	{
		free(arr->data);
		arr->data = 0;
	}
	arr->size = 0;
	arr->cap = 0;
}

void sig_int(int sig)
{
	char msg[] = "\n## 3230yash >> ";
	write(STDOUT_FILENO, msg, sizeof(msg));
}

void print_sig(int sig, char *name)
{
	if (sig == SIGINT)
	{
		printf("%s:  Interrupt\n", name);
	}
	else if (sig == SIGKILL)
	{
		printf("%s:  Killed\n", name);
	}
}

typedef struct
{
	char *tokens[MAX_INPUT_STR];
	int token_count;
	bool should_end;
	bool is_piping;
	int pipe_locations[MAX_INPUT_STR];
	int pipe_count;	
} ParsedInput;

ParsedInput parse_input(char input_stream[MAX_INPUT_CHAR])
{
	ParsedInput result = {
		.should_end = false,
		.is_piping = false,
		.pipe_count = 0,
		.token_count = 0
	};	

	for (int i = 0; i < MAX_INPUT_STR; ++i)
	{
		if (result.tokens[i] != NULL)
		{
			free(result.tokens[i]);
			result.tokens[i] = NULL;
		}
	}

	input_stream[strcspn(input_stream, "\n")] = '\0';

	int stream_index = 0;
	int source_index = stream_index;
	int char_count = 0;

	int token_index = 0;
	int pipe_index = 0;
	bool locked = false;
	while (stream_index < MAX_INPUT_CHAR + 1 &&
		   token_index < MAX_INPUT_STR)
	{
		char c = input_stream[stream_index];
		if (c != 0 && c != '\0' && c != ' ')
		{			
			if (!locked)
			{
				source_index = stream_index;
				locked = true;
			}
			
			++char_count;
		}
		else
		{
			if (locked)
			{
				char *temp = strndup(input_stream + source_index, char_count);
				if (temp)
				{
					if (strcmp(temp, "|") == 0)
					{
						result.pipe_locations[pipe_index++] = token_index;
					}
					result.tokens[token_index++] = temp;
				}
				else
				{
					printf("Out of memory\n"); // NOTE; weird
					exit(-1);
				}
				
				locked = false;
				char_count = 0;
			}
		}
		
		++stream_index;
	}		
	result.token_count = token_index;
	
#if 0
	printf("input tokens: ");
	for (int i = 0; i < MAX_INPUT_STR; ++i)
	{
		if (result.tokens[i])
		{
			printf("%s ", result.tokens[i]);
		}
	}
	printf("\n");
#endif

	result.pipe_count = pipe_index;
	result.is_piping = true;
	if ((result.pipe_count > 0 && result.pipe_locations[0] == 0) ||
		(result.pipe_locations[result.pipe_count - 1] == result.token_count-1))
	{
		printf("3230yash: Incorrect pipe sequence\n");
		result.is_piping = false;
	}
	else if (result.pipe_count > 1)
	{
		for (int pi = 1; pi < result.pipe_count; ++pi)
		{			
			if (result.pipe_locations[pi] - result.pipe_locations[pi - 1] == 1)
			{
				printf("3230yash: should not have two consecutive | without in-between command\n");
				result.is_piping = false;
				break;
			}
		}
	}
		
	if (result.tokens[1] == NULL && (result.tokens[0] && strcmp(result.tokens[0], "exit") == 0))
	{
		result.should_end = true;
	}   	

	return result;
}

void exec_single(ParsedInput parsed)
{
	pid_t pid;
#if 0
	printf("input tokens: ");
	for (int i = 0; i < MAX_INPUT_STR; ++i)
	{
		if (parsed.tokens[i])
		{
			printf("%s ", parsed.tokens[i]);
		}
	}
	printf("\n");
#endif

	if (parsed.tokens[0] == NULL || strcmp(parsed.tokens[0], "|") == 0)
	{
		pid = -1;
	}
	else if (strcmp(parsed.tokens[0], "cd") == 0) {
		printf("3230yash: Unsupported command\n");
		pid = -1;
	}
	else
	{
		pid = fork();
	}
	
	if (pid < 0)
	{
		printf("## 3230yash >> ");
		return;
	}
	else if (pid == 0)
	{
		signal(SIGINT, SIG_DFL);
		if (parsed.tokens[0] == NULL)
		{
			exit(0);
		}

		execvp(parsed.tokens[0], parsed.tokens);
		perror("3230yash");
		exit(1);
	}
	else if (pid > 0)
	{
		signal(SIGINT, SIG_IGN);
		
		int status;
		pid_t caught_pid = waitpid(pid, &status, 0);

		signal(SIGINT, sig_int);
		
		if (WIFSIGNALED(status))
		{
			int sig = WTERMSIG(status);
			print_sig(sig, parsed.tokens[0]);
		}

		if (WIFEXITED(status))
		{
#if 0
			printf("Child %jd exited normally with code: %d\n",
				   (intmax_t)caught_pid, WEXITSTATUS(status));
#endif
			printf("## 3230yash >> ");
		}
		else if (WIFSIGNALED(status))
		{
#if 0
			printf("Child %d was terminated by signal: %d\n", 
				   caught_pid, WTERMSIG(status));
#endif
		}
	}
}

void exec_pipeline(ParsedInput parsed)
{
    int argv_count = parsed.pipe_count + 1;
    char **token_sets[argv_count];
    int token_index = 0;

    for (int pi = 0; pi < argv_count; ++pi) {
        int start = (pi == 0) ? 0 : parsed.pipe_locations[pi-1] + 1;
        int end = (pi == parsed.pipe_count) ? parsed.token_count : parsed.pipe_locations[pi];
        int args_count = end - start;

        char **argv = malloc((args_count + 1) * sizeof(char *));
        memcpy(argv, parsed.tokens + token_index, args_count * sizeof(char *));
        argv[args_count] = NULL;
        token_index += args_count + 1;

        token_sets[pi] = argv;
    }

    int pipefd[2];
    int prev_read = -1;
    pid_t child_pids[argv_count];   // store all child PIDs
    int child_count = 0;

    for (int pi = 0; pi < argv_count; ++pi) {
        if (pi < argv_count - 1) {
            if (pipe(pipefd) == -1) {
                printf("pipe setup failed\n");
                return;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
			signal(SIGINT, SIG_DFL);

            // Child process			
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (pi < argv_count - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            execvp(token_sets[pi][0], token_sets[pi]);
            // --- CHANGE: unified error message for ALL stages ---
            perror("3230yash");
            exit(1);
        }
        else if (pid < 0) {
            printf("unexpected error\n");
            printf("## 3230yash >> ");
            return;
        }
        else {
            // Parent process
            child_pids[child_count++] = pid;

            if (prev_read != -1) close(prev_read);
            if (pi < argv_count - 1) {
                close(pipefd[1]);
                prev_read = pipefd[0];
            }
        }
    }

	signal(SIGINT, SIG_IGN);
    // --- CHANGE: wait for all children AFTER pipeline is built ---
    for (int i = 0; i < child_count; ++i) {
        int status;
        waitpid(child_pids[i], &status, 0);

		if (WIFSIGNALED(status))
		{
			int sig = WTERMSIG(status);
			print_sig(sig, parsed.tokens[0]);
		}				
    }

	signal(SIGINT, sig_int);
	
    printf("## 3230yash >> ");  // prompt after pipeline finishes

    // Free argv arrays
    for (int i = 0; i < argv_count; ++i) {
        free(token_sets[i]);
    }
}

void exec_watch(ParsedInput parsed)
{
	pid_t pid;

	StatArr stat_arr = init_arr(1);
	bool end_monitoring = false;				
	if (parsed.tokens[1] == NULL || strcmp(parsed.tokens[1], "|") == 0)
	{
		printf("3230yash: Invalid watch command\n");
		pid = -1;
	}	
	else
	{
		pid = fork();
	}
	
	if (pid < 0)
	{
		printf("## 3230yash >> ");
		end_monitoring = true;
		return;
	}
	else if (pid == 0)
	{
		signal(SIGINT, SIG_DFL);
		if (parsed.tokens[1] == NULL)
		{
			exit(0);
		}

		execvp(parsed.tokens[1], parsed.tokens+1);
		perror("3230yash");
		exit(1);
	}
	else if (pid > 0)
	{
		signal(SIGINT, SIG_IGN);

		while (!end_monitoring)
		{
			char path[64];
			snprintf(path, sizeof(path), "/proc/%d/stat", pid);
			FILE *f = fopen(path, "r");

			MonitorStat read_stat;
			int pid_read;
			char state;
			unsigned long minflt, majflt, utime, stime, vsize;
			int cpuid;
			if (fscanf(f,
					   "%d %*s %c "
					   "%*d %*d %*d %*d %*d "
					   "%*u "
					   "%lu %*d %lu %*d "   
					   "%lu %lu "
					   "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d "
					   "%lu "
					   "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d "
					   "%d",
					   &pid_read, &state,
					   &minflt, &majflt,
					   &utime, &stime,
					   &vsize,
					   &cpuid) == 8)							
			{
				fclose(f);
				
				read_stat.pid_read = pid_read;  
				read_stat.state = state;   
				read_stat.utime = utime;   
				read_stat.stime = stime;   
				read_stat.vsize = vsize;   
				read_stat.minflt = minflt;   
				read_stat.majflt = majflt;   
				read_stat.cpuid = cpuid;   

				insert_arr(&stat_arr, read_stat);
			}

			int status;
			pid_t caught_pid = waitpid(pid, &status, WNOHANG); // NOTE; handle returned state

			if (caught_pid == pid)
			{
				if (WIFEXITED(status))
				{
					// child exited normally
					printf("\n");
					print_stat(stat_arr);
					printf("## 3230yash >> ");
					waitpid(pid, NULL, 0);   // reap zombie
					end_monitoring = true;
				}
				else if (WIFSIGNALED(status))
				{
					int sig = WTERMSIG(status);
					print_sig(sig, parsed.tokens[1]);  // use the actual command name
					printf("\n");
					print_stat(stat_arr);
					printf("## 3230yash >> ");
					waitpid(pid, NULL, 0);   // reap zombie
					end_monitoring = true;
				}				
			}
			
			if (!end_monitoring)
			{
				usleep(500000);
			}
		}

		signal(SIGINT, sig_int);
	}				
}

int main(void)
{
	signal(SIGINT, sig_int);
	
	char *path_val = getenv("PATH");
#if 0 
	if (path_val)
	{
		printf("PATH values: %s\n", path_val);
	}
	else
	{
		printf("PATH not found\n");
	}
#endif
	
	pid_t shell_pid = getpid();		
	char input_stream[MAX_INPUT_CHAR];

	bool should_end = false;
	printf("## 3230yash >> ");
	while (!should_end)
	{
		memset(input_stream, 0, MAX_INPUT_CHAR);	
		if (fgets(input_stream, MAX_INPUT_CHAR, stdin) == 0)
		{
			clearerr(stdin);
			continue;
		}

		ParsedInput parsed = parse_input(input_stream);
		should_end = parsed.should_end;
		
		if (parsed.should_end)
		{
			break;
		}
		
		if (parsed.tokens[0] && strcmp(parsed.tokens[0], "watch") == 0)
		{
			exec_watch(parsed);
		}
		else if (parsed.is_piping)
		{
			exec_pipeline(parsed);
		}
		else
		{
			if (parsed.pipe_count == 0)
			{
				exec_single(parsed);			
			}
			else
			{
				printf("## 3230yash >> ");
			}
		}

		for (int i = 0; i < MAX_INPUT_STR; ++i)
		{
			if (parsed.tokens[i] != NULL)
			{
				free(parsed.tokens[i]);
			}
		}
	}
	
	if (should_end)
	{
		printf("3230yash: Terminated\n");					
		exit(0);
	}
	
	return 0;
}
