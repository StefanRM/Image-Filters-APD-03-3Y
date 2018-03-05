#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// all info that must be known from a pgm file
typedef struct pgm_info {
	char version[3]; // version of pgm
	char* comments_section1;
	char* comments_section2;
    int width;
    int height;
    int max_val;
    int** data;
} pgm_info;

// read from a pgm file while creating a new pgm file in which all whitespaces are converted to "\n"
// and there are no comments
void read_from_pgm(char* filename, char* aux_filename, pgm_info* pgm_file)
{
	FILE* fp;
	FILE* aux_fp;
	char buffer[100];

	fp = fopen(filename, "rt");
	if (fp == NULL)
	{
		printf("File %s cannot be opened\n", filename);
		exit(0);
	}
	aux_fp = fopen(aux_filename, "wt");
	if (aux_fp == NULL)
	{
		printf("File %s cannot be opened\n", aux_filename);
		exit(0);
	}

	pgm_file->comments_section1 = (char*) calloc(200, sizeof(char));
	pgm_file->comments_section2 = (char*) calloc(200, sizeof(char));

	// read and write till reaching first comment section
	fgets(buffer, sizeof(buffer), fp);
	buffer[strlen(buffer) - 1] = '\0';
	fprintf(aux_fp, "%s\n", buffer);

	// skip comments
	char ch = fgetc(fp);
	while (ch == '#')
	{
        fseek(fp, -1, SEEK_CUR);
        fgets(buffer, sizeof(buffer), fp);
        strcat(pgm_file->comments_section1, buffer);
        ch = fgetc(fp);
	}

	// read and substitute whitespaces with "\n" when writing to aux pgm file
	while (ch != '#' && !feof(fp))
	{
		if (ch == ' ' || ch == '\t')
		{
			fprintf(aux_fp, "\n");
		}
		else
		{
       		fprintf(aux_fp, "%c", ch);
       	}
       	ch = fgetc(fp);
    }

    // skip comments
    while (ch == '#' && !feof(fp))
    {
        fseek(fp, -1, SEEK_CUR);
        fgets(buffer, sizeof(buffer), fp);
        strcat(pgm_file->comments_section2, buffer);
        ch = fgetc(fp);
	}

	// read and substitute whitespaces with "\n" when writing to aux pgm file
	while (ch != '#' && !feof(fp))
	{
		if (ch == ' ' || ch == '\t')
		{
			fprintf(aux_fp, " ");
		}
		else
		{
       		fprintf(aux_fp, "%c", ch);
       	}
       	ch = fgetc(fp);
    }

    fclose(fp);
    fclose(aux_fp);

    // read from aux file that has been created
    aux_fp = fopen(aux_filename, "rt");
	if (aux_fp == NULL)
	{
		printf("File %s cannot be opened\n", aux_filename);
		exit(0);
	}

	fgets(buffer, sizeof(buffer), aux_fp);
	sscanf(buffer, "%s", pgm_file->version); // read version of pgm file

	fgets(buffer, sizeof(buffer), aux_fp);
	sscanf(buffer, "%d", &(pgm_file->width)); // read width

	fgets(buffer, sizeof(buffer), aux_fp);
	sscanf(buffer, "%d", &(pgm_file->height)); // read height

	fgets(buffer, sizeof(buffer), aux_fp);
	sscanf(buffer, "%d", &(pgm_file->max_val)); // read max_val

	// allocate data for pgm info
	int i, j;
	pgm_file->data = (int**) malloc(pgm_file->height * sizeof(int*));
	for(i = 0; i < pgm_file->height; i++)
	{
		pgm_file->data[i] = (int*) malloc(pgm_file->width * sizeof(int));
	}

	// read data from pgm file
	for (i = 0; i < pgm_file->height; i++)
	{
		for (j = 0; j < pgm_file->width; j++)
		{
			fgets(buffer, sizeof(buffer), aux_fp);
			sscanf(buffer, "%d", &(pgm_file->data[i][j]));
		}
	}

	fclose(aux_fp);
	remove(aux_filename); // delete aux file
}

// write a new pgm file with the comments of the input pgm file (only data part differs)
void write_to_pgm(char *filename, pgm_info* pgm_file)
{
	FILE* fp;
	fp = fopen(filename, "wt");
	if (fp == NULL)
	{
		printf("File %s cannot be opened\n", filename);
		exit(0);
	}

	// write all info
	fprintf(fp, "%s\n", pgm_file->version);
	fprintf(fp, "%s", pgm_file->comments_section1);
	fprintf(fp, "%d %d\n", pgm_file->width, pgm_file->height);
	fprintf(fp, "%s", pgm_file->comments_section2);
	fprintf(fp, "%d\n", pgm_file->max_val);

	int i, j;
	for (i = 0; i < pgm_file->height; i++)
	{
		for (j = 0; j < pgm_file->width; j++)
		{
			fprintf(fp, "%d\n", pgm_file->data[i][j]);
		}
	}

	// deallocate all memory space
	for(i = 0; i < pgm_file->height; i++)
	{
		free(pgm_file->data[i]);
	}

	free(pgm_file->data);
	free(pgm_file->comments_section1);
	free(pgm_file->comments_section2);

	fclose(fp);
}