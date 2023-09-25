#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>


int main(int argc, char* argv[])
{

    //open connection for system logging
    openlog("Logs" ,LOG_PID, LOG_USER);

    syslog(LOG_INFO, "Start logging");

    if(argc < 3 )
    {
        //log missing input paramters
        syslog(LOG_ERR, "missing input paramters");
        return 1;
    }
    else if(argc > 3 )
    {
        //log too many input parameters
        syslog(LOG_ERR, "too many input parameters");
        return 1;

    }
    
    char* filepath = argv[1];

    char* message = argv[2];

    // to test the error path 
   // FILE* file = fopen(filepath, "rb");

    FILE* file = fopen(filepath, "w+");
    if(file != NULL)
    {
        syslog(LOG_DEBUG , "Writing %s to file %s ", message, filepath);
        fprintf(file, "%s",message);
    }
    else
    {
         syslog(LOG_ERR, "Error in opening the file ");
         perror("Error return from fopen");
         printf("print Error return from fopen: %s \n", strerror(errno));
    }
  
 
    fclose(file);
    closelog();

    return 0; 
}



