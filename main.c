#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>



int main(int argc, char **argv) 
{
	// File handling vars 
	DIR* FD;
	struct dirent* in_file;
	FILE *common_file;
	FILE *entry_file;
	char buffer[BUFSIZ];

	// Entity strings to match
	const char STRING_INTERMISSION[37] 		= "\"classname\" \"info_intermission\"\n";
	const char STRING_SPAWN[42] 			= "\"classname\" \"info_player_deathmatch\"\n";
	const char STRING_ORIGIN[10]			= "\"origin\"";
	const char STRING_ANGLES[10]			= "\"angle\"";
	const char STRING_MANGLES[10]			= "\"mangle\"";

	// Used to check file extensions
	char *fileExtension;
	const char dot = '.';

	common_file = fopen("cam_positions.csv", "w");

	if (common_file == NULL)
	{
		fprintf(stderr, "Error : Failed to open common_file - %s\n", strerror(errno));

		return 1;
	}

	if (NULL == (FD = opendir (argv[1])))
	{
		fprintf(stderr, "Error : Failed to open input directory (%s) - %s\n", argv[1], strerror(errno));
		fclose(common_file);

		return 1;
	}

	// Print headers in output file
	//fprintf(common_file, "map,x,y,z,pitch,yaw,roll\n");

	while ((in_file = readdir(FD)))
	{
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
		if (entry_file == NULL)
		{
			fprintf(stderr, "Error : Failed to open entry file (%s) - %s\n", in_file->d_name, strerror(errno));
			fclose(common_file);

			return 1;
		}

		// If we ended up here we managed to open the file
		printf("Reading: %s\n", fullpath);

		// We don't bother checking the content
		// until we reach the worldspawn entity...
		bool isBelowJunkChars = 0;

		// 
		bool curItemIsIntermission = 0;
		char curAngles[BUFSIZ];
		char curPosition[BUFSIZ];

		char curPosX[16];
		char curPosY[16];
		char curPosZ[16];

		char curPitch[16];
		char curYaw[16];
		char curRoll[16];

		bool foundOpenTag = 0;

		// To parse the CSV
		char tempBuf[100];
		char seps[] = "\" ";
		char *token;
		int field = 0;

		// fgets reads a line at a time and places result in buffer
		while (fgets(buffer, BUFSIZ, entry_file) != NULL) {

			if (isBelowJunkChars == 0) {
				if (strcmp(buffer, "\"classname\" \"worldspawn\"\n") == 0) {
					isBelowJunkChars = 1;
				}
			} else {
				// We found an entity opening tag
				if (strcmp(buffer, "{\n") == 0) {
					foundOpenTag = 1;

				// We found an entity closening tag,
				// increment what we have found
				} else if ((strcmp(buffer, "}\n") == 0) && foundOpenTag == 1) {
					
					// Shells
					if (curItemIsIntermission) {

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
						
						
						printf("%s,%s,%s,%s,%s,%s,%s\n", 
							in_file->d_name,
							curPosX,
							curPosY,
							curPosZ,
							curPitch,
							curYaw,
							curRoll 
							);

						fprintf(common_file, "%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n", 
							in_file->d_name,
							strtof(curPosX, NULL),
							strtof(curPosY, NULL),
							strtof(curPosZ, NULL),
							strtof(curPitch, NULL),
							strtof(curYaw, NULL),
							strtof(curRoll, NULL) 
							);
						curItemIsIntermission = 0;
						curPosX[0] = curPosY[0] = curPosZ[0] = curPitch[0] = curYaw[0] = curRoll[0] = 0;
					}
					
					foundOpenTag = 0;

				} else {
						
					if (strcmp(buffer, STRING_INTERMISSION) == 0 || strcmp(buffer, STRING_SPAWN) == 0) 	{ curItemIsIntermission = 1; }
					
					if (strncmp(buffer, STRING_ORIGIN, 8) == 0) {
						strncpy(curPosition, buffer, BUFSIZ);
					}

					// For some reason the angles can be named either angles or mangles in bsp files...
					if (strncmp(buffer, STRING_ANGLES, 7) == 0 || strncmp(buffer, STRING_MANGLES, 8) == 0) {
						strncpy(curAngles, buffer, BUFSIZ);
					}
					
				}
			}
		}

		free(fullpath);

		fclose(entry_file);		
	}

	fclose(common_file);

}


