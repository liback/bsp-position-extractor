#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

int lineIsPosition(char buffer[]);
char *stripFileExtension(char* filename);

// Note: TF types not yet used!
enum posTypes {
    POS_INTERMISSION,
    POS_SPAWN_DM,
    POS_SPAWN_CTF_T1,
    POS_SPAWN_CTF_T2,
    POS_SPAWN_TEAM,
    POS_SPAWN_TF_T1,
    POS_SPAWN_TF_T2,
    POS_SPAWN_TF_T3,
    POS_SPAWN_TF_T4
};

const char* posTypes[] = { "intermission", "spawn-dm", "spawn-ctf-t1", "spawn-ctf-t2", "placeholder", "spawn-tf-t1", "spawn-tf-t2", "spawn-tf-t3", "spawn-tf-t4" };

// Entity strings to match
const char STRING_INTERMISSION[36]      = "\"classname\" \"info_intermission\"";
const char STRING_SPAWN[41]             = "\"classname\" \"info_player_deathmatch\"";
const char STRING_SPAWN_TEAM[35]        = "\"classname\" \"info_player_teamspawn\"";
const char STRING_SPAWN_TEAM_ALT[19]    = "\"classname\" \"i_p_t\"";
const char STRING_SPAWN_CTF_T1[32]      = "\"classname\" \"info_player_team1\"";
const char STRING_SPAWN_CTF_T2[32]      = "\"classname\" \"info_player_team2\"";

const char STRING_TF_T1[13]             = "\"team_no\" \"1\"";
const char STRING_TF_T2[13]             = "\"team_no\" \"2\"";
const char STRING_TF_T3[13]             = "\"team_no\" \"3\"";
const char STRING_TF_T4[13]             = "\"team_no\" \"4\"";

const char STRING_ORIGIN[10]            = "\"origin\"";
const char STRING_ANGLE[7]              = "\"angle\"";
const char STRING_ANGLES[8]             = "\"angles\"";
const char STRING_MANGLE[8]             = "\"mangle\"";

/*
Replaces the dot of the filename with NULL
and returns the new, file-extension-less string.
 */
char *stripFileExtension(char* filename) 
{
    char *retstr;
    char *lastdot;

    if (filename == NULL)
        return NULL;

    if ((retstr = malloc(strlen(filename) + 1)) == NULL)
        return NULL;

    strcpy(retstr,filename);
    lastdot = strrchr(retstr, '.');
    if (lastdot != NULL)
        *lastdot = '\0';

    return retstr;
}

/*
Checks if the current line in buffer is
an intermission or spawn position.
 */
int lineIsPosition(char buffer[]) 
{
    if (strstr(buffer, STRING_INTERMISSION)) {
        return POS_INTERMISSION;
    } else if (strstr(buffer, STRING_SPAWN)) {
        return POS_SPAWN_DM;
    } else if (strstr(buffer, STRING_SPAWN_CTF_T1)) {
        return POS_SPAWN_CTF_T1;
    } else if (strstr(buffer, STRING_SPAWN_CTF_T2)) {
        return POS_SPAWN_CTF_T2;
    } else if (strstr(buffer, STRING_SPAWN_TEAM)) {
        return POS_SPAWN_TEAM;
    } else if (strstr(buffer, STRING_SPAWN_TEAM_ALT)) {
        return POS_SPAWN_TEAM;
    }

    return -1;
}



int main(int argc, char **argv) 
{
    // File handling vars 
    DIR* FD;
    struct dirent* in_file;
    FILE *common_file;
    FILE *entry_file;
    char buffer[BUFSIZ];

    // Used to check file extensions
    char *fileExtension;
    const char dot = '.';

    // We don't bother checking the content
    // until we reach the worldspawn entity...
    bool isBelowJunkChars = 0;

    char *curMap;

    int curPosType = 0;

    bool curItemIsPosition = 0;
    char curAngles[BUFSIZ];
    char curPosition[BUFSIZ];

    char curPosX[16];
    char curPosY[16];
    char curPosZ[16];

    char curPitch[16];
    char curYaw[16];
    char curRoll[16];

    int curTeam = 0;

    bool foundOpenTag = 0;

    // To parse the CSV
    char tempBuf[100];
    char seps[] = "\" ";
    char *token;
    int field = 0;
    int mapCount = 0;
    int positionCount = 0;

    common_file = fopen("cam_positions.csv", "w");

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);

        return 1;
    }

    if (common_file == NULL) {
        fprintf(stderr, "Error: Failed to open common_file - %s\n", strerror(errno));

        return 1;
    }

    if (NULL == (FD = opendir (argv[1]))) {
        fprintf(stderr, "Error: Failed to open input directory (%s) - %s\n", argv[1], strerror(errno));
        fclose(common_file);

        return 1;
    }

    // Print headers in output file
    //fprintf(common_file, "map,x,y,z,pitch,yaw,roll\n");

    while ((in_file = readdir(FD))) {
        // Skip unix folder names
        if (!strcmp(in_file->d_name, "."))
            continue;
        if (!strcmp(in_file->d_name, ".."))
            continue;

        fileExtension = strrchr(in_file->d_name, dot);
        
        // Only look for bsp files
        if (!fileExtension || strcmp(fileExtension, ".bsp"))
            continue;

        char *fullpath = malloc(strlen(argv[1]) + strlen(in_file->d_name) + 2);
        sprintf(fullpath, "%s/%s", argv[1], in_file->d_name);

        entry_file = fopen(fullpath, "rw");
        if (entry_file == NULL) {
            fprintf(stderr, "Error: Failed to open entry file (%s) - %s\n", in_file->d_name, strerror(errno));
            fclose(common_file);

            return 1;
        }

        // If we ended up here we managed to open the file
        mapCount++;
        printf("Reading: %s\n", fullpath);

        // fgets reads a line at a time and places result in buffer
        while (fgets(buffer, BUFSIZ, entry_file) != NULL) {

            if (isBelowJunkChars == 0) {
                if (strncmp(buffer, "\"classname\" \"worldspawn\"", 24) == 0) {
                    isBelowJunkChars = 1;
                }
            } else {
                // We found an entity opening tag
                if (strncmp(buffer, "{", 1) == 0) {
                    foundOpenTag = 1;

                // We found an entity closening tag,
                // increment what we have found
                } else if ((strncmp(buffer, "}", 1) == 0) && foundOpenTag == 1) {
                    
                    if (curItemIsPosition) {

                        field = 0;
                        token = strtok(curPosition, seps);

                        while (token != NULL) {
                            if (field == 1)
                                strcpy(curPosX, token);

                            if (field == 2)
                                strcpy(curPosY, token);

                            if (field == 3)
                                strcpy(curPosZ, token);

                            token = strtok(NULL, seps);

                            field++;
                        }


                        field = 0;
                        token = strtok(curAngles, seps);
                        while (token != NULL) {
                            if (field == 1)
                                strcpy(curPitch, token);

                            if (field == 2)
                                strcpy(curYaw,token);

                            if (field == 3)
                                strcpy(curRoll,token);
                            
                            token = strtok(NULL, seps);

                            field++;
                        }

                        if (field < 3) {
                            // If we have no angle value then zero them all
                            sprintf(curPitch,"%i", 0);
                            sprintf(curYaw,"%i", 0);
                            sprintf(curRoll,"%i", 0);
                        } else if (field < 4) {
                            // If we only have one angle value we guess that it's
                            // really the yaw value.
                            strcpy(curYaw, curPitch);
                            sprintf(curPitch,"%i", 0);
                            sprintf(curRoll,"%i", 0);
                        }
                        
                        curMap = stripFileExtension(in_file->d_name);

                        if (curPosType == POS_SPAWN_TEAM) {
                            if (curTeam == 1) {
                                curPosType = POS_SPAWN_TF_T1;
                            } else if (curTeam == 2) {
                                curPosType = POS_SPAWN_TF_T2;
                            } else if (curTeam == 3) {
                                curPosType = POS_SPAWN_TF_T3;
                            } else if (curTeam == 4) {
                                curPosType = POS_SPAWN_TF_T4;
                            }
                        }

                        printf("%s,%s,%s,%s,%s,%s,%s,%s\n", 
                            curMap,
                            curPosX,
                            curPosY,
                            curPosZ,
                            curPitch,
                            curYaw,
                            curRoll,
                            posTypes[curPosType]
                            );

                        fprintf(common_file, "%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%s\n", 
                            curMap,
                            strtof(curPosX, NULL),
                            strtof(curPosY, NULL),
                            strtof(curPosZ, NULL),
                            strtof(curPitch, NULL),
                            strtof(curYaw, NULL),
                            strtof(curRoll, NULL),
                            posTypes[curPosType]
                            );

                        positionCount++;

                        free(curMap);
                        curItemIsPosition = 0;

                        curPosX[0] = curPosY[0] = curPosZ[0] = curPitch[0] = curYaw[0] = curRoll[0] = curTeam = 0;
                    }
                    
                    curAngles[0] = 0;
                    foundOpenTag = 0;

                } else {
                    if (strncmp(buffer, STRING_TF_T1, 13) == 0) { curTeam = 1; }
                    if (strncmp(buffer, STRING_TF_T2, 13) == 0) { curTeam = 2; }
                    if (strncmp(buffer, STRING_TF_T3, 13) == 0) { curTeam = 3; }
                    if (strncmp(buffer, STRING_TF_T4, 13) == 0) { curTeam = 4; }

                    if (lineIsPosition(buffer) != -1) {
                        curItemIsPosition = 1;
                        curPosType = lineIsPosition(buffer);
                    }
                    
                    if (strncmp(buffer, STRING_ORIGIN, 8) == 0) {
                        strncpy(curPosition, buffer, BUFSIZ);
                    }

                    // For some reason the angles can be named either angles or mangles in bsp files...
                    if (strncmp(buffer, STRING_ANGLE, 7) == 0 || strncmp(buffer, STRING_ANGLES, 8) == 0 || strncmp(buffer, STRING_MANGLE, 8) == 0) {
                        strncpy(curAngles, buffer, BUFSIZ);
                    }
                }
            }
        }
        free(fullpath);
        fclose(entry_file);     
    }
    fclose(common_file);
    printf("Finished reading %i files and saved %i positions.\n", mapCount, positionCount);
}
