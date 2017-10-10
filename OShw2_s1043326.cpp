#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define typeMax 500                                                             //for data type
#define ptrMax 50                                                               //for ptr type
#define color( index ) "\001\033[1;" index "m\002"                              //text color, 32 -> red;34 -> green
#define reset "\001\033[0;0m\002"

void ExecCMD( const int, char**( & ));                                          //prototype of ExecCMD
void GetUser( char*( &buffer )){
    char *saveCWD = new char[ typeMax ]();

    strcat( buffer, color( "32" ));
    strcat( buffer, getenv( "USER" ));
    strcat( buffer, reset "@" color( "34" ));
    strcat( buffer, getcwd( saveCWD, typeMax ));
    strcat( buffer, reset "> " );

    delete [] saveCWD;
}

void SetUserPath( char*( &target )){
    char *tmpArgv = new char[ typeMax ](), *shrink = new char[ typeMax ]();
    swap( target, tmpArgv );

    strcat( target, getenv( "HOME" ));
    shrink = strtok( tmpArgv, "~" );
    if( shrink != NULL ) strcat( target, shrink );

    tmpArgv = shrink = NULL;
    delete [] tmpArgv;
    delete [] shrink;
}

int GetArgc( char**( &argv )){
    int argcCounter = 0;
    char *input = NULL, *tmpArgv = NULL, *user = new char[ typeMax ]();

    GetUser( user );
    input = readline( user );
    add_history( input );

    if( input == NULL ){
        cout << "\033[1;31mEOF\033[0;0m\n";
        argcCounter = -1;
    }
    else{
        tmpArgv = strtok( input, " " );
        while( tmpArgv != NULL ){
            strcpy( argv[ argcCounter++ ], tmpArgv );
            tmpArgv = strtok( NULL, " " );
        }
        for( int i = 0; i < argcCounter; ++i ){
            if(( argv[ i ][ 0 ] == '~' && argv[ i ][ 1 ] == '/' ) ||
               ( argv[ i ][ 0 ] == '~' && argv[ i ][ 1 ] == '|' ) ||
               ( argv[ i ][ 0 ] == '~' && strlen( argv[ i ]) == 1 ))
                SetUserPath( argv[ i ]);
        }
        input = tmpArgv = NULL;
    }

    delete input;
    delete tmpArgv;
    delete [] user;

    return argcCounter;
}

void CreateArgvList( const int size, int*( &argcList ), char***( &argvList )){
    argcList = new int[ size ]();
    argvList = new char**[ size ];
    for( int i = 0; i < size; ++i ){
        argvList[ i ] = new char*[ ptrMax ];
        for( int j = 0; j < ptrMax; ++j )
            argvList[ i ][ j ] = new char[ typeMax ]();
    }
}

void DestroyArgvList( const int size, int*( &argcList ), char***( &argvList )){
    delete argcList;
    for( int i = 0; i < size; ++i ){
        for( int j = 0; j < ptrMax; ++j)
            delete [] argvList[ i ][ j ];
        delete [] argvList[ i ];
    }
    delete [] argvList;
}

void NormalCMD( const int argc, char**( &argv )){
    if( strcmp( argv[ 0 ], "cd" ) == 0 ){                                       //special case
        if( argc >= 2 ){
            if( chdir( argv[ 1 ] ) != 0 )
                cerr << "沒有此一檔案或目錄\n";
        }
        else if( argc == 1 )
            chdir( getenv( "HOME" ));

        return;
    }

    pid_t pid = fork();
    if( pid < 0 ){
        cerr << "Something error!\n";
        exit( 1 );
    }
    else if( pid == 0 ){
        int tmpArgc = argc;
        bool mark = false;

        if( strcmp( argv[ 0 ], "ls" ) == 0 ||
          ( strcmp( argv[ 0 ], "grep" ) == 0 && argc >= 2 )){
            strcpy( argv[ tmpArgc++ ], "--color=auto" );                        //make ls, grep text has color
            mark = true;
        }

        delete [] argv[ tmpArgc ];
        argv[ tmpArgc ] = NULL;

        if( execvp( argv[ 0 ], argv ) == -1 )
            cerr << argv[ 0 ] << "：無此指令\n";

        argv[ tmpArgc ] = new char[ typeMax ]();
        if( mark ){
            delete [] argv[ argc ];
            argv[ argc ] = new char[ typeMax ]();
        }

        exit( 0 );
    }
    else waitpid( pid, NULL, 0 );
}

void PipeCMD( const int argc, char**( &argv )){
    char*** argvList;
    int pipeCounter = 0, *argcList, pFd[ 2 ], FdIn = STDIN_FILENO;

    CreateArgvList( ptrMax, argcList, argvList );
    for( int i = 0; i < argc; ++i ){
        if( strchr( argv[ i ], '|' ) != NULL ){
            if( argv[ i ][ 0 ] == '|' ) pipeCounter++;
            char *tmpArgv = new char[ typeMax ]();
            strcpy( tmpArgv, argv[ i ]);
            char *find = strtok( tmpArgv, "|" );
            while( find != NULL ){
                strcpy( argvList[ pipeCounter ][ argcList[ pipeCounter ]++ ], find );
                find = strtok( NULL, "|" );
                if( find != NULL ) pipeCounter++;
            }
            if( argv[ i ][ strlen( argv[ i ]) - 1 ] == '|' && strlen( argv[ i ]) > 1 )
                pipeCounter++;
            delete [] tmpArgv;
            delete [] find;
        }
        else
            strcpy( argvList[ pipeCounter ][ argcList[ pipeCounter ]++ ], argv[ i ]);
    }pipeCounter++;                             //++ for pipeNum, no ++ for "|"

    pid_t pid[ pipeCounter ];                                                   //use pid_t arr to make sure every pid can wait

    for( int i = 0; i < pipeCounter; ++i ){                                     //pFd[ 1 ] for write-end, pFd[ 0 ] for read-end
        pipe( pFd );
        pid[ i ] = fork();
        if( pid[ i ] < 0 ){
            cerr << "Something error!\n";
            exit( 1 );
        }
        else if( pid[ i ] == 0 ){                                               //child process
            dup2( FdIn, STDIN_FILENO );
            if( i != pipeCounter - 1 )
                dup2( pFd[ 1 ], STDOUT_FILENO );

            ExecCMD( argcList[ i ], argvList[ i ]);

            DestroyArgvList( ptrMax, argcList, argvList );
            close( FdIn );                                                      //in child process, close all Fd
            close( pFd[ 0 ]);
            close( pFd[ 1 ]);
            exit( 0 );
        }
        else{                                                                   //parent process
            if( i != 0 ) close( FdIn );
            close( pFd[ 1 ]);
            if( i == pipeCounter - 1 ) close( pFd[ 0 ]);                        //if last, close Fd
            else FdIn = pFd[ 0 ];                                               //otherwise save pFd[ 0 ]
        }
    }
    for( int i = 0; i < pipeCounter; ++i ) waitpid( pid[ i ], NULL, 0 );        //wait every child process

    DestroyArgvList( ptrMax, argcList, argvList );
}

void RedirectionCMD( const int argc, char**( &argv )){
    char*** argvList;
    int *argcList, fd, stdNum = -1, cNo = -1;

    CreateArgvList( 2, argcList, argvList );
    for( argcList[ 0 ] = argc - 1; argcList[ 0 ] >= 0; --argcList[ 0 ]){
        if( strcmp( argv[ argcList[ 0 ]], ">" ) == 0 || strcmp( argv[ argcList[ 0 ]], "<" ) == 0 ){
            stdNum = ( strcmp( argv[ argcList[ 0 ]], ">" ) == 0 ? STDOUT_FILENO : STDIN_FILENO );
            break;
        }
        else if( strcmp( argv[ argcList[ 0 ]], "1>" ) == 0 || strcmp( argv[ argcList[ 0 ]], "2>" ) == 0 ){
            stdNum = ( strcmp( argv[ argcList[ 0 ]], "1>" ) == 0 ? STDOUT_FILENO : STDERR_FILENO );
            break;
        }
        else if( strcmp( argv[ argcList[ 0 ]], ">>" ) == 0 || strcmp( argv[ argcList[ 0 ]], "2>>" ) == 0 ){
            stdNum = ( strcmp( argv[ argcList[ 0 ]], ">>" ) == 0 ? 3 : 4 );
            break;
        }
        else if( strcmp( argv[ argcList[ 0 ]], "1>&2" ) == 0 || strcmp( argv[ argcList[ 0 ]], "2>&1" ) == 0 )
            cNo = ( strcmp( argv[ argcList[ 0 ]], "1>&2" ) == 0 ? STDOUT_FILENO : STDERR_FILENO );
    }
    for( int i = 0; i < argcList[ 0 ]; ++i ) strcpy( argvList[ 0 ][ i ], argv[ i ]);
    for( int i = argcList[ 0 ] + 1, j = 0; i < argc; ++i ) strcpy( argvList[ 1 ][ j++ ], argv[ i ]);

    pid_t pid = fork();
    if( pid < 0 ){
        cerr << "Something error!\n";
        exit( 1 );
    }
    else if( pid == 0 ){
        if( stdNum == STDIN_FILENO )
            fd = open( argvList[ 1 ][ 0 ], O_RDONLY, 0644 );
        else if( stdNum == STDOUT_FILENO || stdNum == STDERR_FILENO )
            fd = open( argvList[ 1 ][ 0 ], O_CREAT | O_TRUNC | O_WRONLY, 0644 );
        else if( stdNum == 3 || stdNum == 4 ){                                  //append text at last
            fd = open( argvList[ 1 ][ 0 ], O_APPEND | O_WRONLY, 0644 );
            stdNum = ( stdNum == 3 ? STDOUT_FILENO : STDERR_FILENO );
        }
        if( fd < 0 ){
            cerr << "File " << argvList[ 1 ][ 0 ] << " can't open!\n";
            exit( 1 );
        }
        dup2( fd, stdNum );
        if( cNo != -1 ) dup2( fd, cNo );
        close( fd );

        ExecCMD( argcList[ 0 ], argvList[ 0 ]);

        DestroyArgvList( 2, argcList, argvList );

        exit( 0 );
    }
    else waitpid( pid, NULL, 0 );

    DestroyArgvList( 2, argcList, argvList );
}

int GetType( const int argc, char**( &argv )){
    for( int i = argc - 1; i >= 0; --i ){
        if( strcmp( argv[ i ], "<" ) == 0 || strcmp( argv[ i ], "2>" ) == 0 ||
            strcmp( argv[ i ], ">" ) == 0 || strcmp( argv[ i ], "1>" ) == 0 ||
            strcmp( argv[ i ], ">>" ) == 0 || strcmp( argv[ i ], "2>>" ) == 0 ||
            strcmp( argv[ i ], "1>&2" ) == 0 || strcmp( argv[ i ], "2>&1" ) == 0 )
            return 2;
        else if( strchr( argv[ i ], '|' ) != NULL )
            return 1;
    }
    return 0;
}

void ExecCMD( const int argc, char**( &argv )){
    int type = GetType( argc, argv );

    if( type == 0 ) NormalCMD( argc, argv );
    else if( type == 1 ) PipeCMD( argc, argv );
    else if( type == 2 ) RedirectionCMD( argc, argv );
    else cerr << "Something error!\n";
}

bool AnalysisCMD( const int argc, char**( &argv )){
    if( argc < 0 || strcmp( argv[ 0 ], "exit" ) == 0 ){
        cout << "GoodBye~\n";
        return false;
    }
    else if( argc == 0 ) return true;

    ExecCMD( argc, argv );

    return true;
}

int main(){
    cout << "\033[2J\033[3J\033[1;1H";

    int argc = 0;
    char **argv = new char*[ ptrMax ];
    for( int i = 0; i < ptrMax; ++i )
        argv[ i ] = new char[ typeMax ]();

    do{
        argc = GetArgc( argv );
    }while( AnalysisCMD( argc, argv ));

    for( int i = 0; i < ptrMax; ++i )
        delete [] argv[ i ];
    delete [] argv;

    return 0;
}
