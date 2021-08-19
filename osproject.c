

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

//macros
#define BASHPRINT "os_project_shell> "
#define INT_MAX 2147483647
#define MAXVAL 1023
#define READ_PERMISSION 0444
#define WRITE_PERMISSION 0666

/**
 * Control structure for a string tokenizer.  Maintains the
 * tokenizer's state.
 */
typedef struct tokenizer {
  char *str;			/* the string to parse */
  char *pos;			/* position in string */
} TOKENIZER;



/**
 * Initializes the tokenizer
 *
 * @param string the string that will be tokenized.  Should be non-NULL.
 * @return an initialized string tokenizer on success, NULL on error.
 */
TOKENIZER *init_tokenizer( char *string )
{
  TOKENIZER *tokenizer;
  int len;
  assert( string != NULL );

  tokenizer = (TOKENIZER *)malloc(sizeof(TOKENIZER));
  assert( tokenizer != NULL );
  len = strlen(string) + 1;	/* don't forget \0 char */
  tokenizer->str = (char *)malloc(len);
  assert( tokenizer->str != NULL );
  memcpy( tokenizer->str, string, len );
  tokenizer->pos = tokenizer->str;
  return tokenizer;
}



/**
 * Deallocates space used by the tokenizer.
 * @param tokenizer a non-NULL, initialized string tokenizer
 */
void free_tokenizer( TOKENIZER *tokenizer )
{
  assert( tokenizer != NULL );
  free( tokenizer->str );
  free( tokenizer );
}



/**
 * Retrieves the next token in the string.  The returned token is
 * malloc'd in this function, so you should free it when done.
 *
 * @param tokenizer an initiated string tokenizer
 * @return the next token
 */
char *get_next_token( TOKENIZER *tokenizer )
{
  assert( tokenizer != NULL );
  char *startptr = tokenizer->pos;
  char *endptr;
  char *tok;

  if( *tokenizer->pos == '\0' )	/* handle end-case */
    return NULL;

  

  /* if current position is a delimiter, then return it */
  if( (*startptr == '|') || (*startptr == '&') || 
      (*startptr == '<') || (*startptr == '>') ) {
    tok = (char *)malloc(2);
    tok[0] = *startptr;
    tok[1] = '\0';
    tokenizer->pos++;
    return tok;
  }

  while( isspace(*startptr) )	/* remove initial white spaces */
    startptr++;

  if( *startptr == '\0' )
    return NULL;

  /* go until next character is a delimiter */
  endptr = startptr;
  for( ;; ) {
    if( (*(endptr+1) == '|') || (*(endptr+1) == '&') || (*(endptr+1) == '<') ||
	(*(endptr+1) == '>') || (*(endptr+1) == '\0') || (isspace(*(endptr+1))) ) {
      tok = (char *)malloc( (endptr - startptr) + 2 );
      memcpy( tok, startptr, (endptr - startptr) + 1 );
      tok[(endptr - startptr) + 1] = '\0'; /* null-terminate the string */
      tokenizer->pos = endptr + 1;
      while( isspace(*tokenizer->pos) ) /* remove trailing white space */
	tokenizer->pos++;
      return tok;
    }
    endptr++;
  }
  
  assert( 0 );			/* should never reach here */
  return NULL;			/* but satisfy compiler */
}

pid_t pid;

// Write to the buffer
void writeBuffer(int fd, char *buffer)
{
	for (int i = 0; buffer[i] != '\0'; i++)
	{
		if (write(fd, &buffer[i], sizeof(char)) < 0)
		{
			perror("invalid write was not successful\n");
			exit(EXIT_FAILURE);
		}
	}
}

// Read the input command from the user.
void readBuffer(int fd, char *readInput)
{
	char data = '\0';
	int readback = 0;
	int i = 0;
	while (data != '\n')
	{
		// Read byte by byte.
		readback = read(fd, &data, sizeof(char));

		if (readback == -1)
		{
			// Check if read system call gave an error.
			perror("invalid read was not successful\n");
			exit(EXIT_FAILURE);
		}
		else if (readback == 0)
		{
			// Check for EOF and exit.
			writeBuffer(STDOUT_FILENO, "^D\n");
			exit(EXIT_SUCCESS);
		}
		else if (i < MAXVAL)
		{ // Store in the buffer till the max value.
			readInput[i] = data;
			i++;
		}
	}
	// Remove the \r and add a null terminator.
	readInput[--i] = '\0';
}

void getCommand(char *input, char **command)
{
	TOKENIZER *tokenizer;
	char *temp = input;
	char *tok;
	tokenizer = init_tokenizer(temp);

	tok = get_next_token(tokenizer);
	*command = (char *)malloc(sizeof(char) * strlen(tok));
	memset(*command, '\0', sizeof(char) * strlen(tok));
	memcpy(*command, tok, strlen(tok));
	*(*command + strlen(tok)) = '\0';
	free(tok);				   /* free the token now that we're done with it */
	free_tokenizer(tokenizer); /* free memory */
}

bool getArgs(char *input, char **args, char **inputArgs, char **outputArgs, int isFirstCommand, int isLastCommand)
{
	TOKENIZER *tokenizer;
	char *tok;
	tokenizer = init_tokenizer(input);
	int j = 0, inRedirection = 0, outRedirection = 0, endOfCommand = 0;
	int countInputRedirection = 0, countOutputRedirection = 0;

	while ((tok = get_next_token(tokenizer)) != NULL)
	{
		if (inRedirection == 1)
		{
			*inputArgs = (char *)malloc(sizeof(char) * strlen(tok));
			memset(*inputArgs, '\0', sizeof(char) * strlen(tok));
			memcpy(*inputArgs, tok, strlen(tok));
			*(*inputArgs + strlen(tok)) = '\0';
			inRedirection = 0;
		}
		if (outRedirection == 1)
		{
			*outputArgs = (char *)malloc(sizeof(char) * strlen(tok));
			memset(*outputArgs, '\0', sizeof(char) * strlen(tok));
			memcpy(*outputArgs, tok, strlen(tok));
			*(*outputArgs + strlen(tok)) = '\0';
			outRedirection = 0;
		}
		if (tok[0] == '<')
		{
			if ((isLastCommand == 1 && isFirstCommand == 1) || (isFirstCommand == 1 && isLastCommand != 1)) // is the only command without any pipes or is the first command
			{
				inRedirection = 1;
				endOfCommand = 1;
				countInputRedirection++;
			}
			if (isLastCommand == 1 && isFirstCommand != 1) // is the last command
			{
				writeBuffer(STDOUT_FILENO, "Invalid: Multiple input redirections\n");
				return false;
			}
			if (isLastCommand != 1 && isFirstCommand != 1) // is the middle command
			{
				writeBuffer(STDOUT_FILENO, "Invalid: Multiple input redirections\n");
				return false;
			}
		}
		if (tok[0] == '>')
		{
			if ((isLastCommand == 1 && isFirstCommand == 1) || (isFirstCommand != 1 && isLastCommand == 1)) // is the only command without any pipes os is the last command
			{
				outRedirection = 1;
				endOfCommand = 1;
				countOutputRedirection++;
			}
			if (isFirstCommand == 1 && isLastCommand != 1) // is the first command
			{
				writeBuffer(STDOUT_FILENO, "Invalid: Multiple output redirections\n");
				return false;
			}
			if (isLastCommand != 1 && isFirstCommand != 1) // is the middle command
			{
				writeBuffer(STDOUT_FILENO, "Invalid: Multiple input redirections\n");
				return false;
			}
		}

		if (tok[0] != '<' && tok[0] != '>' && endOfCommand == 0)
		{
			args[j] = (char *)malloc(sizeof(char) * strlen(tok));
			memset(args[j], '\0', sizeof(char) * strlen(tok));
			memcpy(args[j], tok, strlen(tok));
			*(args[j] + strlen(tok)) = '\0';
			j++;
		}
		free(tok); /* free the token now that we're done with it */
	}
	free_tokenizer(tokenizer); /* free memory */
	args[j] = NULL;
	if (countInputRedirection > 1)
	{
		writeBuffer(STDOUT_FILENO, "Invalid: Multiple standard input redirects or redirect in invalid location\n");
		return false;
	}
	if (countOutputRedirection > 1)
	{
		writeBuffer(STDOUT_FILENO, "Invalid: Multiple standard output redirects\n");
		return false;
	}
	return true;
}

int getCommands(char *input, char **args)
{
	char *temp;
	int i, j = 0;
	temp = input;

	while(*temp == ' ')
		temp++;
		
	while (*temp != '\0')
	{
		i = 0;
		//Parse each arguments and save in the args pointer.
		char *tempcomm = (char *)malloc(MAXVAL * sizeof(char));

		while (*temp != '|' && *temp != '\0')
		{
			tempcomm[i++] = *temp;
			temp++;
		}

		if (*temp != '\0')
		{
			temp++;
		}

		if (i != 0)
		{
			tempcomm[i] = '\0';
			args[j++] = tempcomm;
		}

		// Free the memory.
		//free(tempcomm);
	}
	args[j] = (char *)0;

	return j;
}

bool setRedirectionFd(char *inputFd, char *outputFd, int inPipefd, int outPipeFd)
{
	int dupOutput = -1;
	if (inPipefd == -1 && outPipeFd == -1) // It is a single command without any pipes
	{
		if (strlen(inputFd) != 0) // Set STDIN_FILENO to input fd provided in the command
		{
			int fdInput = open(inputFd, O_RDONLY, READ_PERMISSION);
			if (fdInput == -1)
			{
				writeBuffer(STDOUT_FILENO, "Invalid standard input redirect: No such file or directory\n");
				return false;
			}
			dupOutput = dup2(fdInput, STDIN_FILENO);
			if (dupOutput == -1)
			{
				perror("Invalid dup");
				return false;
			}
		}
		if (strlen(outputFd) != 0) // Set STDOUT_FILENO to output fd provided in the command
		{
			int fdOutput = open(outputFd, O_WRONLY | O_CREAT | O_TRUNC, WRITE_PERMISSION);
			if (fdOutput == -1)
			{
				writeBuffer(STDOUT_FILENO, "Invalid standard output redirect: File creation failed\n");
				return false;
			}
			dupOutput = dup2(fdOutput, STDOUT_FILENO);
			if (dupOutput == -1)
			{
				perror("Invalid dup");
				return false;
			}
		}
		return true;
	}
	else if (inPipefd != -1 && outPipeFd != -1) // It is a middle command surrounded by pipes
	{
		dupOutput = dup2(inPipefd, STDIN_FILENO); //dup STDIN_FILENO to read end of pipe
		if (dupOutput == -1)
		{
			perror("Invalid dup");
			return false;
		}
		dupOutput = dup2(outPipeFd, STDOUT_FILENO); //dup STDOUT_FILENO to write end of pipe
		if (dupOutput == -1)
		{
			perror("Invalid dup");
			return false;
		}
		return true;
	}
	else if (inPipefd == -1 && outPipeFd != -1) // It is the first command followed by pipes
	{
		if (strlen(inputFd) != 0)
		{
			int fdInput = open(inputFd, O_RDONLY, READ_PERMISSION);
			if (fdInput == -1)
			{
				writeBuffer(STDOUT_FILENO, "Invalid standard input redirect: No such file or directory\n");
				return false;
			}
			dupOutput = dup2(fdInput, STDIN_FILENO);
			if (dupOutput == -1)
			{
				perror("Invalid dup");
				return false;
			}
		}
		dupOutput = dup2(outPipeFd, STDOUT_FILENO);
		if (dupOutput == -1)
		{
			perror("Invalid dup");
			return false;
		}
		return true;
	}
	else if (inPipefd != -1 && outPipeFd == -1) // It is the last command that gets input from pipe
	{
		if (strlen(outputFd) != 0)
		{
			int fdOutput = open(outputFd, O_WRONLY | O_CREAT | O_TRUNC, WRITE_PERMISSION);
			if (fdOutput == -1)
			{
				writeBuffer(STDOUT_FILENO, "Invalid standard output redirect: File creation failed\n");
				return false;
			}
			dupOutput = dup2(fdOutput, STDOUT_FILENO);
			if (dupOutput == -1)
			{
				perror("Invalid dup");
				return false;
			}
		}
		dupOutput = dup2(inPipefd, STDIN_FILENO);
		if (dupOutput == -1)
		{
			perror("Invalid dup");
			return false;
		}
		return true;
	}
	else
	{
		// should have never come here !
	}
	return true;
}

// Handle Ctrl+C.
void ctrlcHandle(int sig)
{
}

// Main function call.
int main(int argc, char *argv[])
{
	//registers a signal ctrl c
	signal(SIGINT, ctrlcHandle);

	while (1)
	{
		// Initialize the variable.
		char input[MAXVAL];
		char *commAll[MAXVAL];
		int numProcess = 0;

		// Write the Prompt statement: penn-shredder#
		writeBuffer(STDOUT_FILENO, (char *)BASHPRINT);

		// Read the command from the user.
		readBuffer(STDIN_FILENO, input);

		int l = 0;

		while(input[l] == ' ')
			l++;

		int commands = getCommands(input, commAll);

		if(commands == 0)
			continue;

		pid_t pidArray[commands];
		int fd[commands - 1][2], i;

		for (i = 0; i < (commands - 1); i++)
		{
			// Open the pipe.
			if (pipe(fd[i]) < 0)
			{
				perror("invalid pipe failed");
				exit(EXIT_FAILURE);
			}
		}

		for (int i = 0; i < commands; i++)
		{
			char *arguments[MAXVAL];
			char *command = NULL, *inputFd = "", *outputFd = "";
			int isLastCommand = 0, isFirstCommand = 0;

			// Parse the input for the command.
			getCommand(commAll[i], &command);

			if (i == (commands - 1))
				isLastCommand = 1;

			if (i == 0)
				isFirstCommand = 1;

			// Parse the arguments for the command.
			bool validCommand = getArgs(commAll[i], arguments, &inputFd, &outputFd, isFirstCommand, isLastCommand);

			if (!validCommand)
				break;

			// Fork a child from parent.
			pid = fork();

			if (pid == 0)
			{
				// Child Process.
				bool allOkay = false;

				if (isLastCommand == 1 && isFirstCommand == 1) // is the only command without any pipes
				{
					allOkay = setRedirectionFd(inputFd, outputFd, -1, -1);
				}
				else if (i == 0) // is the first command followed by a pipe
				{
					close(fd[i][0]);											 // close the read end of the pipe
					allOkay = setRedirectionFd(inputFd, outputFd, -1, fd[i][1]); // set the STDOUT to fd write end
				}
				else if (i == (commands - 1)) // is the last command after a pipe
				{
					close(fd[i - 1][1]);											 // close the write end of the pipe
					allOkay = setRedirectionFd(inputFd, outputFd, fd[i - 1][0], -1); // set the STDIN to fd read end
				}
				else // is the middle command enclosed by pipes on both sides
				{
					// close the read end of the next pipe
					allOkay = setRedirectionFd(inputFd, outputFd, fd[i - 1][0], fd[i][1]); // set the STDIN to fd read end
					close(fd[i - 1][1]);												   // close the write end of the previous pipe
					close(fd[i][0]);
				}
				if (!allOkay)
					break;

				for (int j = 0; j < (commands - 1); j++)
				{

					// Close both the file descriptors.
					close(fd[j][0]);
					close(fd[j][1]);
				}

				int execvpOutput = execvp(command, arguments);
				if (execvpOutput == -1)
				{
					perror("Invalid execvp");
					exit(1);
				}
			}
			else
			{
				// Parent Process
				// Add pid to the list of childs list
				pidArray[i] = pid;
				numProcess++;
			}
			// free(command);
		}

		// Close all the pipes.
		for (int j = 0; j < (commands - 1); j++)
		{

			// Close both the file descriptors.
			close(fd[j][0]);
			close(fd[j][1]);
		}

		for (int i = 0; i < numProcess; i++)
		{
			int status;
			pid_t w = waitpid(pidArray[i], &status, WUNTRACED | WCONTINUED);
			if (w == -1)
			{
				perror("invalid waitpid error");
				exit(EXIT_FAILURE);
			}
		}
	}
}
