#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>

#define TRUE 1
#define FALSE 0
#define STD_INPUT 0
#define STD_OUTPUT 1
#define MAXLINE 64
#define MAXARGS 10
#define LEGALTYPENUMS 4
 
const char prompt[] = "b063040061\033[1;32m@shell:" ;

typedef struct command {
	int argc ;			// argument count
	char **argv ;		// argument array
	char **argvPipe ;	// argument pipe array
	char *INdirName ;	// indirection name
	char *OUTdirName ;	// outdirection name
	int bg ;			// do in background
	char type[LEGALTYPENUMS] ;	// N(ormal), E(rror), |, <, > initial to N
	enum builtin_t {			// built in function
		NONE, QUIT, CD			// history
	} builtin ;
} command ;

char *ReadCmdLine() {
	// read all line
	char *line = NULL ;
	size_t bufsize = 0 ;
	getline( &line, &bufsize, stdin ) ;	
	// printf("%s", line ) ;
	return line ;
} // ReadCmdLine()

command *Parse( char *cmdLine ) {
	// parse to command struct
	command *cmd = malloc( sizeof( *cmd ) ) ;
	int numsOfArg = 0, typeC = 0, i, tlen= 0, j, pipecount = 0 ;
	char temp[MAXLINE] ;
	cmd->argc = 0 ;								// ----initial------
	cmd->bg = FALSE ;
	cmd->type[0] = 'N' ;
	cmd->builtin = NONE ;						// 		  |
	// cmd->piotype = NORMAL ;
	cmd->argv = malloc( MAXLINE*MAXARGS ) ;
	cmd->argvPipe = malloc( MAXLINE*MAXARGS ) ;	// ----initial------
	for ( i = 0 ; i < MAXLINE ; i++ )
		temp[i] = '\0' ;
	for ( i = 0 ; i < strlen( cmdLine ) ; i++ ) {
		if ( cmdLine[i] != ' ' && cmdLine[i] != '|' && cmdLine[i] != '&' &&\
			 cmdLine[i] != '<' && cmdLine[i] != '>' &&\
			 cmdLine[i] != '\t' && i+1 != strlen( cmdLine )  )
			temp[tlen++] = cmdLine[i] ;
		else {
			// if ( i+1 == strlen( cmdLine ) && cmdLine[i] != ' ' && cmdLine[i] != '\t')
			// 	temp[tlen++] = cmdLine[i] ;
			if ( cmd->type[0] != 'N' && strcmp( cmd->type, ">" ) != 0 &&\
				 strcmp( cmd->type, "<" ) != 0 && strcmp( cmd->type, "|" ) != 0 &&\
				 strcmp( cmd->type, "><" ) != 0 && strcmp( cmd->type, "<>" ) != 0 &&\
				 strcmp( cmd->type, "|>" ) != 0 && strcmp( cmd->type, "<|" ) != 0 &&\
				 strcmp( cmd->type, "<|>" ) != 0 ) {
				// strcpy( cmd->errorMessage, "error to use '<', '>', or '|'\n" );
				cmd->type[0] = 'E' ;	// ERROR
				// printf("ERROR") ;
				break ;
			} // end if
			if ( cmdLine[i] == '&' ) {		// do in background
				int a = TRUE ;
				for ( j = i+1 ; j < strlen( cmdLine ) ; j++ )
					if ( cmdLine[j] != ' ' && cmdLine[j] != '\t' && cmdLine[j] != '\n' )
							a = FALSE ;
				if ( a == TRUE ) {
					cmd->bg = TRUE ;
					// printf("bg") ;
				}
				else {
					// printf("ERROR") ;
					// strcpy( cmd->errorMessage, "Error to use '&'\n" ) ;
					cmd->type[0] = 'E' ;
					break ;
				} // end else
			} // end if
			if ( tlen != 0 ) {			// fill in command pasrse
				if ( cmd->type[0] == 'N' ) {
					if ( strcmp( temp, "cd" ) == 0 )
						cmd->builtin = CD ;
					if ( strcmp( temp, "exit" ) == 0 )
						cmd->builtin = QUIT ;
					cmd->argv[cmd->argc] = malloc( MAXLINE ) ;
					strcpy( cmd->argv[cmd->argc], temp ) ;
					cmd->argc++ ;
					// printf( "N:%s \n", temp ) ;
				} // end if
				else if ( cmd->type[typeC-1] == '|' ) {	// pipe
					cmd->argvPipe[pipecount] = malloc( MAXLINE ) ;
					strcpy( cmd->argvPipe[pipecount], temp ) ;
					pipecount++ ;
					// printf( "Pipe:%s %d\n", temp, pipecount ) ;
				} // end else if
				else if ( cmd->type[typeC-1] == '<' ) {	// indir
					if ( cmd->INdirName == NULL ) {
						cmd->INdirName = malloc( strlen( temp )+1 ) ;
						strcpy( cmd->INdirName, temp ) ;
						// printf( "in:%s \n", temp ) ;
					} // end if
				} // end else if
				else {									// outdir
					if ( cmd->OUTdirName == NULL ) {
						cmd->OUTdirName = malloc( strlen( temp )+1 ) ;
						strcpy( cmd->OUTdirName, temp ) ;
						// printf( "out:%s \n", temp ) ;
					} // end if
				} // end else
				for ( tlen = 0 ; tlen < MAXLINE ; tlen++ )
						temp[tlen] = '\0' ;
				tlen = 0 ;
			} // end if
			if ( cmdLine[i] == '|' || cmdLine[i] == '<' || cmdLine[i] == '>' )
				cmd->type[typeC++] = cmdLine[i] ;
		} // end else
	} // end for
	// printf("%sqq\n", cmd->type ) ;
	return cmd ;
} // Parse()

void ExcuteCmd( command *cmd ) {
	// all types process "|", "<", ">", "|>", "<|", "<|>", "<>", "><" 8 types
	pid_t pid, pidexp ;	// normal expand
	int pipefd[2], status, nfd ;	// pipe file descriptor, normal file descriptor
	pid = fork() ;
	if ( pid < 0 ) 
		perror( "Error : fork failed 1\n" ) ;
	else if ( pid == 0 ) {
		// "|", "<|", "<|>", "|>"
		if ( cmd->type[0] == '|' || cmd->type[1] == '|' ) {
			if ( pipe( &pipefd[0] ) < 0 )
				perror( "Error : pipe failed\n" ) ;
			pidexp = fork() ;
			if ( pidexp < 0 )
				perror( "Error : fork failed 2(pipe)\n" ) ;
			else if ( pidexp == 0 ) {
				close( pipefd[0] ) ;
				close( STD_OUTPUT ) ;	//	--->>>
				dup( pipefd[1] ) ;		// same as dup2( pipefd[1], STD_OUTPUT )
				close( pipefd[1] ) ;
				// "<|", "<|>"
				if ( cmd->type[0] == '<' && cmd->type[1] == '|' ) {
					pipefd[0] = open( cmd->INdirName, O_RDONLY ) ;
					// get an index from fd table point to IndirName
					close( STD_INPUT ) ;// --->>> 
					dup( pipefd[0] ) ;	// same as dup2( pipdfd[0], STD_INPUT )
					close( pipefd[0] ) ;
				} // end if
				if ( execvp( cmd->argv[0], cmd->argv )  < 0 ) {
					fprintf( stderr, "%s: Command not found pc1\n", cmd->argv[0] ) ;
					exit( EXIT_FAILURE ) ;	
				} // end if
			} // end else if
			else {		// still in pipe
				waitpid( pidexp, &status, 0 ) ;
				close( pipefd[1] ) ;
				close( STD_INPUT ) ;
				dup ( pipefd[0] ) ;
				close( pipefd[0] ) ;
				// "<|>", "|>"
				if ( cmd->type[1] == '>' || cmd->type[2] == '>' ) {
					nfd = open( cmd->OUTdirName, O_WRONLY | O_CREAT | O_TRUNC, 0666 ) ;
					dup2( nfd, STD_OUTPUT ) ;
					close( nfd ) ;
				} // end if
				if ( execvp( cmd->argvPipe[0], cmd->argvPipe )  < 0 ) {
					fprintf( stderr, "%s: Command not found pc2\n", cmd->argvPipe[0] ) ;
					exit( EXIT_FAILURE ) ;	
				} // end if
			} // end else
		} // end if
		else {
			// "<", "<>", "><"
			if ( cmd->type[0] == '<' || cmd->type[1] == '<' ) {
				nfd = open( cmd->INdirName, O_RDONLY ) ;	// get index // Read Only 
				dup2( nfd, STD_INPUT ) ;
				close( nfd ) ;
			} // end if
			// ">", "><", "<>"
			if ( cmd->type[0] == '>' || cmd->type[1] == '>' ) {
				nfd = open( cmd->OUTdirName, O_WRONLY | O_CREAT | O_TRUNC, 0666 ) ;	// rw
				dup2( nfd, STD_OUTPUT ) ;
				close( nfd ) ;
			} // end if
			if ( execvp( cmd->argv[0], cmd->argv ) < 0 ) {
				fprintf( stderr, "%s: Command not found pp1\n", cmd->argv[0] ) ;
				exit( EXIT_FAILURE ) ;	
			} // end if
		} // end else
	} // end else if
	else {
		// parent wait or do in background
		if ( cmd->bg == TRUE )
			printf( "[%s][PID] : %u\n", cmd->argv[0], pid ) ;
		else
			waitpid( pid, &status, 0 ) ;
	} // end else
	
} // ExcuteCmd

void Cd( command *cmd ) {
	// change direction
	char *curDir = get_current_dir_name() ;
	if ( cmd->argc == 1 ) {
		curDir = "/home" ;
		chdir(curDir) ;
	} // end if
	else {
		if ( chdir( cmd->argv[1] ) != 0 )
			fprintf( stderr, "Error: \'%s\'is unknown.\n", cmd->argv[1] ) ;
	} // end else
	
} // CD()

int Evaluate( command *cmd ) {
	// builtin command or error or excute command
	if ( cmd->builtin == NONE ) {
		if ( cmd->type[0] == 'E' ) {
			printf( "Error to use '|', '>', '<', or '&'\n" ) ;
			// printf("print error message\n") ;
			return 1 ;
		} // end if
		ExcuteCmd( cmd ) ;
		return 1 ;
	}
	else if ( cmd->builtin == CD ) {
		Cd( cmd ) ;
		return 1 ;
	}
	else if ( cmd->builtin == QUIT )
		return 0 ;
	else {
		printf( "ERROR\n" ) ;
		return 1 ;
	}
} // Evaluate()

void Shell() {
	command *cmd ;
	char *cmdLine, *curDir ; // hostName[256] ;
	while( TRUE ) {
		curDir = get_current_dir_name() ;
		// gethostname( hostName, 256 ) ;
		printf( "%s\033[1;34m%s\033[0;m$ ", prompt, curDir ) ;
		cmdLine = ReadCmdLine() ;
		// printf( "%s", cmdLine ) ;
		// cmdLine[strlen( cmdLine )-1] = '\0' ;
		// Evaluate( cmdLine ) ;
		// Parse( cmdLine ) ;
		cmd = Parse( cmdLine ) ;
		if ( Evaluate( cmd ) == 0 )
			return ;
		// ExcuteCmd( cmd ) ;
	} // end while
} // shell()

int main() {
	
	Shell() ;
	
} // main()

