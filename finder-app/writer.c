#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

bool check_arguments(int argc, char *argv[])
{
    if (argc == 1) {
        syslog(LOG_ERR, "Usage: %s writefile writestr\n", argv[0]);
        return false;
    }
    if (argc < 3) {
        syslog(LOG_ERR, "Too few arguments.\n");
        return false;
    }
    if (argc > 3) {
        syslog(LOG_ERR, "Too many arguments.\n");
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);

    if (!check_arguments(argc, argv)) {
        closelog();
        return EXIT_FAILURE; 
    }

    FILE* file = fopen(argv[1], "w");

    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open file %s. %m.\n", argv[1]);
        closelog();
        return EXIT_FAILURE;
    }

    int rc = fputs(argv[2], file);

    if (rc == EOF) {
        syslog(LOG_ERR, "Failed to write to file %s. %m.\n", argv[1]);
        closelog();
        fclose(file);
        return EXIT_FAILURE;
    }

    syslog(LOG_DEBUG, "Writing %s to %s\n", argv[2], argv[1]);

    closelog();
    fclose(file);

    return EXIT_SUCCESS;
}
