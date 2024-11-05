#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "nixi.h"
#include <sys/wait.h>

void help(char *programname)
{
    printf("Usage: %s <commmand> [argumuents for the command]\n\n", programname);
    printf("<command>\n");
    printf("    info - Gives info about packages\n");
    printf("    search - Search packages\n");
    printf("    refresh - Refreshes the repo\n");
    printf("    install - Installs a new package\n");
    printf("    uninstall - Uninstalls a package\n");
    printf("    update - Updates packages to new version\n");
    printf("    list - Lists packages");
    printf("    run - Runs a package\n");
}

char* expand_tilde(const char* path)
{
    if (path[0] == '~')
    {
        const char* home = getenv("HOME");
        if (home)
        {
            size_t home_len = strlen(home);
            size_t path_len = strlen(path);
            char* expanded_path = malloc(home_len + path_len);
            if (expanded_path)
            {
                strcpy(expanded_path, home);
                strcpy(expanded_path + home_len, path + 1);
                return expanded_path;
            }
        }
    }
    return strdup(path);
}

int dependency_used(char* dependency)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt;
    char query[1024];
    char* dependency_tok;
    int rc;

    rc = sqlite3_open(expand_tilde("~/.nixi/installed.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Installed database not found. Perhaps you skipped one part of the installation?\n");
        exit(-1);
    }

    snprintf(query, sizeof(query), "select * from package");
    if(sqlite3_prepare_v2(db, query, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        exit(-1);
    }

    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 3) != NULL)
        {
            dependency_tok = strtok((char*)sqlite3_column_text(stmt, 3), ",");
            while(dependency_tok != NULL)
            {
                if(strcmp(dependency_tok, dependency) == 0)
                {
                    return 1;
                }
                dependency_tok = strtok(NULL, ",");
            }
        }
    }
    
    return 0;
}

int info(char* package, char* programname)
{
    sqlite3 *db = NULL;
    int rc;

    rc = sqlite3_open(expand_tilde("~/.nixi/packages.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Database doesn't exist or it is broken. Try '%s refresh'.\n", programname);
        return 1;
    }

    sqlite3_stmt *stmt;
    char *query = "select * from packages where name=?";
    sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, package, -1, SQLITE_STATIC);
    printf("Name: %s\n", package);
    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 3) != NULL)
        {
            printf("Version: %s\n", sqlite3_column_text(stmt, 3));
        }
        if(sqlite3_column_text(stmt, 2) != NULL)
        {
            printf("Description: %s\n", sqlite3_column_text(stmt, 2));
        }
    }

    return 0;
}

int search(char* package, char* programname)
{
    sqlite3 *db = NULL;
    int rc;
    sqlite3_stmt *stmt;
    char query[512];
    char temp[512];

    rc = sqlite3_open(expand_tilde("~/.nixi/packages.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Database doesn't exist or it is broken. Try '%s refresh'.\n", programname);
        return 1;
    }

    snprintf(query, sizeof(query), "select * from packages where name like ?");
    snprintf(temp, sizeof(query), "%%%s%%", package);
    if(sqlite3_prepare_v2(db, query, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    if(sqlite3_bind_text(stmt, 1, temp, -1, SQLITE_STATIC) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 1;
    }
    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 2) != NULL)
        {
            printf("%s\n", sqlite3_column_text(stmt, 1));
        }
    }

    return 0;
}

size_t writeDataOnStream(void * buffer, size_t size, size_t nbytes, void * stream)
{
    size_t bytes_written = fwrite( buffer, size, nbytes, (FILE *) stream);
    return bytes_written;
}

int copy(char* attributes)
{
    FILE *fptr1;
    FILE *fptr2;
    char* token;
    char* file1;
    char* file2;
    int c;
    struct stat fileStat;
    const char *homedir = getenv("HOME");
    if(homedir == NULL)
    {
        perror("getenv");
        return 1;
    }

    token = strtok(attributes, " ");
    char* resolved1 = expand_tilde(token);
    if(resolved1 == NULL)
    {
        perror("realpath");
        return EXIT_FAILURE;
    }
    file1 = malloc(strlen(resolved1) + 1);
    strlcpy(file1, resolved1, strlen(resolved1) + 1);

    token = strtok(NULL, " ");
    char* resolved2 = expand_tilde(token);
    if(resolved2 == NULL)
    {
        perror("realpath");
        return EXIT_FAILURE;
    }
    file2 = malloc(strlen(resolved2) + 1);
    strlcpy(file2, resolved2, strlen(resolved2) + 1);

    fptr1 = fopen(file1, "r");
    if(fptr1 == NULL)
    {
        perror("fopen");
        return 1;
    }

    fptr2 = fopen(file2, "w");
    if(fptr2 == NULL)
    {
        perror("fopen");
        return 1;
    }

    while((c = fgetc(fptr1)) != EOF)
    {
        fputc(c, fptr2);
    }

    if(stat(file1, &fileStat) < 0)
    {
        perror("stat");
        return 1;
    }

    if(chmod(file2, fileStat.st_mode) < 0)
    {
        perror("chmod");
        return 1;
    }

    fclose(fptr1);
    fclose(fptr2);
    free(file1);
    free(file2);

    return 0;
}

int download_package(char* token, char* url)
{
    CURL *curl;
    CURLcode res;
    FILE* curl_file_save;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl)
    {
        curl_file_save = fopen(token, "wb");
        if(curl_file_save == NULL)
        {
            perror("Failed to open file");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return 1;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataOnStream);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl_file_save);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            printf("cURL error: %s\n", curl_easy_strerror(res));
            return 1;
        }

        fclose(curl_file_save);
        curl_easy_cleanup(curl);
    }
    else
    {
        return 1;
    }

    curl_global_cleanup();

    return 0;
}

int rename_nixi(char* attributes)
{
    char file1[256];
    char file2[256];
    char* token;
    token = strtok(attributes, " ");
    strlcpy(file1, token, sizeof(file1));
    token = strtok(NULL, " ");
    strlcpy(file2, token, sizeof(file2));

    return rename(file1, file2);
}

int install_package(FILE **fptr)
{
    char attributes[1024];
    char *token_cmd;
    char *cmd;
    size_t i;
    char nixi_install_file[1024];

    while(fgets(nixi_install_file, 1024, *fptr))
    {
        memset(attributes, 0, sizeof(attributes));
        token_cmd = strtok(nixi_install_file, " ");
        if(token_cmd == NULL)
        {
            continue;
        }
        cmd = malloc(strlen(token_cmd) + 1);
        if(cmd == NULL)
        {
            perror("malloc");
            exit(-1);
        }
        strlcpy(cmd, token_cmd, strlen(token_cmd) + 1);
        while(token_cmd != NULL)
        {
            token_cmd = strtok(NULL, " ");
            if(token_cmd == NULL)
            {
                break;
            }
            strncat(attributes, token_cmd, sizeof(attributes)-1);
            strncat(attributes, " ", sizeof(attributes)-1);
        }

        for(i=0; i<strlen(attributes); i++)
        {
            if(attributes[i]==13 || attributes[i]==10)
            {
                attributes[i]=0;
                break;
            }
        }
        if(strcmp(cmd, "EXECUTE") == 0)
        {
            system(attributes);
        }
        else if(strcmp(cmd, "CHANGEDIRECTORY") == 0)
        {
            if(chdir(attributes) != 0)
            {
                perror("chdir");
                exit(-1);
            }
        }
        else if(strcmp(cmd, "COPY") == 0)
        {
            if(copy(attributes) != 0)
            {
                exit(-1);
            }
        }
        else if(strcmp(cmd, "MAKEDIRECTORY") == 0)
        {
            if(mkdir(attributes, 0755) != 0)
            {
                exit(-1);
            }
        }
        else if(strcmp(cmd, "RENAME") == 0 || strcmp(cmd, "MOVE") == 0)
        {
            if(rename_nixi(attributes) != 0)
            {
                exit(-1);
            }
        }
        free(cmd);
    }

    fclose(*fptr);

    return 0;
}

int commitdatabase(char *token, char *files, char* dependency, char* version)
{
    char query[1024];
    sqlite3 *db2 = NULL;
    sqlite3_stmt *stmt2;
    char* error_message = 0;
    int rc;

    rc = sqlite3_open(expand_tilde("~/.nixi/installed.db"), &db2);
    if(rc != SQLITE_OK)
    {
        db2 = NULL;
        printf("Installed database not found. Perhaps you skipped one part of the installation?\n");
        return 1;
    }
    snprintf(query, sizeof(query), "select * from package where name=?");
    if(sqlite3_prepare_v2(db2, query, -1, &stmt2, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db2));
        exit(-1);
    }

    if(sqlite3_bind_text(stmt2, 1, token, -1, SQLITE_STATIC) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db2));
    }

    snprintf(query, sizeof(query), "insert into package (name, files, dependency, version) values ('%s', '%s', '%s', '%s')", token, files, dependency, version);
    rc = sqlite3_exec(db2, query, 0, 0, &error_message);
    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Error creating data: %s\n", error_message);
        sqlite3_free(error_message);
        exit(-1);
    }

    return 0;
}

int extractpackage(FILE** fptr, char* token)
{
    char command[300];
    snprintf(command, sizeof(command), "nixi-%s", token);
    if(mkdir(command, 0755) != 0)
    {
        perror("mkdir");
        exit(-1);
    }
    snprintf(command, sizeof(command), "nixi-%s/%s.tar.gz", token, token);
    if(rename(token, command) != 0)
    {
        perror("rename");
        exit(-1);
    }
    snprintf(command, sizeof(command), "nixi-%s", token);
    if(chdir(command) != 0)
    {
        perror("chdir");
        exit(-1);
    }
    snprintf(command, sizeof(command), "%s.tar.gz", token);
    extract(command, 1, ARCHIVE_EXTRACT_TIME);
    snprintf(command, sizeof(command), "NIXI_INSTALL");

    *fptr = fopen(command, "r");
    if(*fptr == NULL)
    {
        printf("Package is broken.\n");
        perror("fopen");
        return 1;
    }

    return 0;
}

int install(char* token, char* programname, int commit)
{
    sqlite3 *db = NULL;
    int rc;
    sqlite3_stmt *stmt;
    char query[1024];
    char sig1temp[128];
    char sig2temp[128];
    char* dependency_tok;
    FILE *fptr;

    rc = sqlite3_open(expand_tilde("~/.nixi/packages.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Database doesn't exist or it is broken. Try '%s refresh'.\n", programname);
        return 1;
    }

    snprintf(query, sizeof(query), "select * from packages where name=?");
    if(sqlite3_prepare_v2(db, query, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    if(sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 4) != NULL)
        {
            snprintf(sig1temp, sizeof(sig1temp), "/tmp/%s", token);
            download_package(sig1temp, (char*)sqlite3_column_text(stmt, 4));
            snprintf(sig1temp, sizeof(sig1temp), "/tmp/%s.tar.gz.sig", token);
            snprintf(sig2temp, sizeof(sig2temp), "%s.sig", (char*)sqlite3_column_text(stmt, 4));
            download_package(sig1temp, sig2temp);
        }
        else
        {
            perror("cURL");
            exit(-1);
        }
        
        if(sqlite3_column_text(stmt, 6) != NULL)
        {
            dependency_tok = strtok((char*)sqlite3_column_text(stmt, 6), ",");
            while(dependency_tok != NULL)
            {
                install(dependency_tok, programname, commit);
                dependency_tok = strtok(NULL, ",");
            }
        }

        snprintf(sig2temp, sizeof(sig2temp), "/tmp/%s", token);
        if(check_sig(sig1temp, sig2temp) == 0)
        {
            printf("Keys not verified. Exiting...\n");
            fflush(stdout);
            exit(-1);
        }
        
        chdir("/tmp");
        extractpackage(&fptr, token);
        install_package(&fptr);
        if(commit == 1) commitdatabase(token, (char*)sqlite3_column_text(stmt, 5), (char*)sqlite3_column_text(stmt, 6), (char*)sqlite3_column_text(stmt, 3));
    }

    return 0;
}

int refresh(void)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "localhost:8000/packages.db");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fopen(expand_tilde("~/.nixi/packages.db"), "wb"));
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
        {
            printf("cURL error: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    return 0;
}

int run(char* package[], int argcount)
{
    int i;
    char attributes[512];
    char package_execute[1024];
    memset(attributes, 0, sizeof(attributes));
    for(i=3;i<argcount;i++)
    {
        strncat(attributes, package[i], sizeof(attributes) - strlen(attributes) - 1);
        strncat(attributes, " ", sizeof(attributes) - strlen(attributes) - 1);
    }
    strlcpy(package_execute, "~/.nixi/", sizeof(package_execute));
    strncat(package_execute, package[2], sizeof(package_execute) - strlen(attributes) - 1);
    strncat(package_execute, " ", sizeof(package_execute) - strlen(attributes) - 1);
    strncat(package_execute, attributes, sizeof(package_execute) - strlen(attributes) - 1);
    system(package_execute);

    return 0;
}

int uninstall(char* package, char* programname, int commit)
{
    sqlite3 *db = NULL;
    sqlite3 *db2 = NULL;
    int rc, found;
    sqlite3_stmt *stmt;
    sqlite3_stmt *stmt2;
    char query[1024];
    char *token;
    char* error_message = 0;
    char* dependency_tok;

    rc = sqlite3_open(expand_tilde("~/.nixi/packages.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Database doesn't exist or it is broken. Try '%s refresh'.\n", programname);
        return 1;
    }
    
    snprintf(query, sizeof(query), "select * from packages where name=?");
    if(sqlite3_prepare_v2(db, query, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    if(sqlite3_bind_text(stmt, 1, package, -1, SQLITE_STATIC) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 5) != NULL)
        {
            token = strtok((char*)sqlite3_column_text(stmt, 5), ",");
            while(token != NULL)
            {
                remove(expand_tilde(token));
                token = strtok(NULL, ",");
            }

            if(commit == 1)
            {
                rc = sqlite3_open(expand_tilde("~/.nixi/installed.db"), &db2);
                if(rc != SQLITE_OK)
                {
                    db2 = NULL;
                    printf("Installed database not found. Perhaps you skipped one part of the installation?\n");
                    return 1;
                }
                
                snprintf(query, sizeof(query), "select * from package where name=?");
                if(sqlite3_prepare_v2(db2, query, -1, &stmt2, 0) != SQLITE_OK)
                {
                    printf("SQL error: %s\n", sqlite3_errmsg(db2));
                    return 1;
                }
                if(sqlite3_bind_text(stmt2, 1, package, -1, SQLITE_STATIC) != SQLITE_OK)
                {
                    printf("SQL error: %s\n", sqlite3_errmsg(db2));
                    return 1;
                }

                found = 0;
                while((rc = sqlite3_step(stmt2)) == SQLITE_ROW)
                {
                    if(sqlite3_column_text(stmt, 2) != NULL)
                    {
                        found = 1;
                    }
                }

                if(found == 1)
                {
                    snprintf(query, sizeof(query), "delete from package where name like '%s'", package);
                    rc = sqlite3_exec(db2, query, 0, 0, &error_message);
                    if(rc != SQLITE_OK)
                    {
                        printf("SQL error: %s\n", sqlite3_errmsg(db2));
                        return 1;
                    }
                }
                sqlite3_finalize(stmt2);
            }
        }
        else
        {
            printf("Package is broken.\n");
            return 1;
        }

        if(sqlite3_column_text(stmt, 6) != NULL)
        {
            dependency_tok = strtok((char*)sqlite3_column_text(stmt, 6), ",");
            while(dependency_tok != NULL)
            {
                if(dependency_used(dependency_tok) == 1) uninstall(dependency_tok, programname, commit);
                dependency_tok = strtok(NULL, ",");
            }
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

int update(char* programname)
{
    sqlite3 *db = NULL;
    sqlite3 *db2 = NULL;
    sqlite3_stmt *stmt;
    sqlite3_stmt *stmt2;
    int rc;
    char query1[512];
    char query2[512];

    rc = sqlite3_open(expand_tilde("~/.nixi/installed.db"), &db2);
    if(rc != SQLITE_OK)
    {
        db2 = NULL;
        printf("Installed database not found. Perhaps you skipped one part of the installation?\n");
        return 1;
    }

    refresh();

    rc = sqlite3_open(expand_tilde("~/.nixi/packages.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Database doesn't exist or it is broken. Try '%s refresh'.\n", programname);
        return 1;
    }

    strlcpy(query1, "select * from package", sizeof(query1));
    if(sqlite3_prepare_v2(db2, query1, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 4) != NULL)
        {
            snprintf(query2, sizeof(query2), "select * from packages where name = ?");
            if(sqlite3_prepare_v2(db, query2, -1, &stmt2, 0) != SQLITE_OK)
            {
                printf("SQL error: %s\n", sqlite3_errmsg(db));
                return 1;
            }

            if(sqlite3_bind_text(stmt2, 1, (char*)sqlite3_column_text(stmt, 1), -1, SQLITE_STATIC) != SQLITE_OK)
            {
                printf("SQL error: %s\n", sqlite3_errmsg(db));
                return 1;
            }

            while((rc = sqlite3_step(stmt2)) == SQLITE_ROW)
            {
                if(sqlite3_column_text(stmt2, 3) != NULL)
                {
                    if(strcmp((char*)sqlite3_column_text(stmt2, 3), (char*)sqlite3_column_text(stmt, 4)) != 0)
                    {
                        uninstall((char*)sqlite3_column_text(stmt2, 1), programname, 0);
                        install((char*)sqlite3_column_text(stmt2, 1), programname, 0);
                    }
                }
            }
            sqlite3_finalize(stmt2);
        }
    }

    sqlite3_finalize(stmt);

    return 0;
}

int list(void)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt;
    char query[512];
    int rc, total = 0;

    rc = sqlite3_open(expand_tilde("~/.nixi/installed.db"), &db);
    if(rc != SQLITE_OK)
    {
        db = NULL;
        printf("Installed database not found. Perhaps you skipped one part of the installation?\n");
        return 1;
    }

    strlcpy(query, "select * from package", sizeof(query));
    if(sqlite3_prepare_v2(db, query, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if(sqlite3_column_text(stmt, 1) != NULL)
        {
            printf("%s -> Version %s\n", (char*)sqlite3_column_text(stmt, 1), (char*)sqlite3_column_text(stmt, 4));
            total++;
        }
    }
    sqlite3_finalize(stmt);

    printf("%d packages in total.\n", total);

    return 0;
}

int main(int argc, char* argv[])
{
    int i;

    if(argc < 2)
    {
        help(argv[0]);
        return 1;
    }

    if(strcmp(argv[1], "info") == 0)
    {
        return info(argv[2], argv[0]);
    }
    else if(strcmp(argv[1], "refresh") == 0)
    {
        return refresh();
    }
    else if(strcmp(argv[1], "search") == 0)
    {
        return search(argv[2], argv[0]);
    }
    else if(strcmp(argv[1], "install") == 0)
    {
        for(i = 2; i < argc; i++)
        {
            pid_t pid = fork();

            if(pid < 0)
            {
                perror("fork");
                exit(1);
            } 
            else if(pid == 0)
            {
                install(argv[i], argv[0], 1);
                exit(0);
            }
        }
        
        for(i = 0; i < argc - 2; i++)
        {
            wait(NULL);
        }

        return 0;
    }
    else if(strcmp(argv[1], "uninstall") == 0)
    {
        return uninstall(argv[2], argv[0], 1);
    }
    else if(strcmp(argv[1], "run") == 0)
    {
        return run(argv, argc);
    }
    else if(strcmp(argv[1], "update") == 0)
    {
        return update(argv[0]);
    }
    else if(strcmp(argv[1], "list") == 0)
    {
        return list();
    }
    else
    {
        printf("No such command '%s'.", argv[1]);
        return 1;
    }
}
