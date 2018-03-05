#include <mpi.h>
#include "process_pgm.h"

#define BUFF_SIZE 100
#define FILTER_NAME_SIZE 20
#define FILE_NAME_SIZE 30
#define SOBEL_TAG 0
#define MEAN_REMOVAL_TAG 1
#define TERMINATE_TAG 2

// typical Linked List structure
typedef struct List {
	int info; // id of process
	int height; // height of data
	int width; // width of data
	int index_of_start; // index where data starts from parent
	struct List* next;
} TList;

// every node neighbors
typedef struct Links {
	int parent;
    int size; // size of list
    TList* list;
} TLinks;

// Sobel X filter applied to a pixel and its neighbors (3 x 3 matrix)
int apply_sobel_filter(int* mat_in)
{
	int i, pixel = 0;
	int sobel[9] = { 1, 0, -1, 2, 0, -2, 1, 0, -1 }; // convolution matrix
	for (i = 0; i < 9; i++)
	{
		pixel += mat_in[i] * sobel[i];
	}

	pixel += 127;
	if (pixel > 255) 
	{
		pixel = 255;
	}

	if (pixel < 0)
	{
		pixel = 0;
	}

	return pixel;
}

// Mean Removal filter applied to a pixel and its neighbors (3 x 3 matrix)
int apply_mean_removal_filter(int* mat_in)
{
	int i, pixel = 0;
	int mean_removal[9] = { -1, -1, -1, -1, 9, -1, -1, -1, -1 }; // convolution matrix
	for (i = 0; i < 9; i++)
	{
		pixel += mat_in[i] * mean_removal[i];
	}

	if (pixel > 255) 
	{
		pixel = 255;
	}

	if (pixel < 0)
	{
		pixel = 0;
	}

	return pixel;
}

int main(int argc, char * argv[])
{
	int rank; // process ID
	int nProcesses; // number of processes
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

	if(argc != 4) // correct usage of program
	{
		printf("Incorrect number of parameters\n");
		MPI_Finalize();
		exit(0);
	}

	TLinks links; // every connection of current node
	// nodes don't know anything about topology
	links.size = 0;
	links.parent = -1;
	links.list = NULL;

	// topology reading
	FILE* fp;
	char buffer[BUFF_SIZE];
	fp = fopen(argv[1], "rt");
	if (fp == NULL)
	{
		printf("File %s cannot be opened\n", argv[1]);
		exit(0);
	}

	int i, j;
	for (i = 0; i < rank; i++) // skip lines
	{
		fgets(buffer, sizeof(buffer), fp);
	}

	// read only current node's neighbors
	fgets(buffer, sizeof(buffer), fp);
	char *ret;

	// parse topology file line
    ret = strchr(buffer, ':');
    ret[0] = ' ';
    ret = strchr(buffer, '\n'); // add an additional -1 at the end of the line
    if (ret != NULL)
    {
	    ret[0] = ' ';
	    ret[1] = '-';
	    ret[2] = '1';
	    ret[3] = '\0';
	}
	else
	{
		ret = strchr(buffer, '\0');
		ret[0] = ' ';
	    ret[1] = '-';
	    ret[2] = '1';
	    ret[3] = '\0';
	}
	
	// create node's neighbors list
	int child;
	TList* p;
	// read the next neighbor and save the remaining buffer
	sscanf(buffer, "%d%[^\n]s", &child, buffer);
	while (1)
	{
		// read the next neighbor and save the remaining buffer
		sscanf(buffer, "%d%[^\n]s", &child, buffer);
		if (child == -1)
		{
			break;
		}

		links.size++; // list gets bigger
		TList* node = (TList*) malloc(sizeof(TList));
		node->info = child;
		node->next = NULL;

		// add at the end of the list
		if (links.size == 1)
		{
			links.list = node;
		}
		else
		{
			p->next = node;
		}

		p = node;
	}

	fclose(fp); // close topology file


	int toSend; // amount of data to send
	int buffSent[2]; // first message has two values (height and width to allocate)
	MPI_Status status[nProcesses]; // status for detecting informations while receiving messages
	int data_width, data_height; // dimensions of data matrix for the current process
	int* data_stream; // data for input
	int* data_computed; // data that has been computed
	pgm_info my_data; // all info from pgm
	int mat[9]; // pixel with its neighbors matrix
	int nr_filters = -1, current_filter_nr = 0; // how many filters will be applied, number of current filter
	char filter[FILTER_NAME_SIZE], file_in[FILE_NAME_SIZE], file_out[FILE_NAME_SIZE]; // filter name and files names
	int tag; // current tag
	int* statistics = (int*) calloc(nProcesses, sizeof(int)); // info about lines processed in topology
	int* statistics_aux = (int*) calloc(nProcesses, sizeof(int)); // where the information is received

	// process 0 is the initiator, so it opens the file with the tasks
	if (rank == 0)
	{
		fp = fopen(argv[2], "rt");
		if (fp == NULL)
		{
			printf("File %s cannot be opened\n", argv[2]);
			exit(0);
		}

		fgets(buffer, sizeof(buffer), fp);
		sscanf(buffer, "%d", &nr_filters); // reading the number of filters to be applied
	}

	while (1)
	{
		if (rank == 0)
		{
			current_filter_nr++; // new filter to apply
			if (current_filter_nr <= nr_filters) // did we apply all filters?
			{
				fgets(buffer, sizeof(buffer), fp);
				sscanf(buffer, "%s %s %s", filter, file_in, file_out);
				if (strcmp(filter, "sobel") == 0) // check which filter to be applied (having only 2 filters)
				{
					tag = SOBEL_TAG;
				}
				else
				{
					tag = MEAN_REMOVAL_TAG;
				}

				read_from_pgm(file_in, "aux.pgm", &my_data); // read info about the pgm file
				// dimensions of data
				data_height = my_data.height;
				data_width = my_data.width;

				toSend = my_data.height / links.size; // amount to be sent
				int sent = 0; // total amount sent
				buffSent[0] = toSend; // nr of lines from matrix
				buffSent[1] = my_data.width; // width of matrix

				// all initiator's neighbors are children of the process
				p = links.list; // send the same amount of lines for all children, except the last one
				for (i = 0; i < links.size - 1; i++)
				{
					MPI_Send(buffSent, 2, MPI_INT, p->info, tag, MPI_COMM_WORLD);
					// dimensions of data in each child
					p->height = toSend;
					p->width = data_width;
					sent += toSend;
					p = p->next; // next child
				}

				// last child gets the remaining lines
				toSend = my_data.height - sent;
				buffSent[0] = toSend; // remaining lines
				MPI_Send(buffSent, 2, MPI_INT, p->info, tag, MPI_COMM_WORLD);
				p->height = toSend;
				p->width = data_width;
			}
			else
			{
				// all filters applied, so the initiator announces the children to stop and return statistics
				tag = TERMINATE_TAG;
				buffSent[0] = -1;
				buffSent[1] = -1;
				p = links.list; // announces children
				while (p != NULL)
				{
					MPI_Send(buffSent, 2, MPI_INT, p->info, tag, MPI_COMM_WORLD);
					p = p->next;
				}
				break; // exit the loop
			}
		}
		else
		{
			// the message received can be the dimensions for data allocation or termination
			MPI_Recv(buffSent, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status[rank]);
			tag = status[rank].MPI_TAG; // get tag
			data_height = buffSent[0];
			data_width = buffSent[1];

			// if the parent is not yet determined
			if (links.parent == -1)
			{
				links.parent = status[rank].MPI_SOURCE; // source of message is parent
				p = links.list; // eliminate parent from children list
				if (p->info == links.parent)
				{
					links.list = p->next;
					links.size--;
					free(p);
				}
				else
				{
					while (p->next->info != links.parent)
					{
						p = p->next;
					}

					TList* q = p->next;
					p->next = q->next;
					links.size--;
					free(q);
				}
			}

			if (links.size > 0) // send the message with tag to children
			{
				toSend = data_height / links.size;
				int sent = 0;
				p = links.list;
				buffSent[0] = toSend;

				for (i = 0; i < links.size - 1; i++)
				{
					MPI_Send(buffSent, 2, MPI_INT, p->info, tag, MPI_COMM_WORLD);
					p->height = toSend;
					p->width = data_width;
					sent += toSend;
					p = p->next;
				}
				toSend = data_height - sent;
				buffSent[0] = toSend;
				MPI_Send(buffSent, 2, MPI_INT, p->info, tag, MPI_COMM_WORLD);
				p->height = toSend;
				p->width = data_width;
			}

			if (tag == TERMINATE_TAG) // if termination tag then exit the loop
			{
				break;
			}
		}

		// allocate data to be received
		data_stream = (int*) malloc((data_height + 2) * data_width * sizeof(int));
		// allocate data to be computed
		data_computed = (int*) malloc(data_height * data_width * sizeof(int));

		if (rank == 0) // initiator process data of file
		{
			for (j = 0; j < data_width; j++) // add zero values for first and last rows
			{
				data_stream[j] = 0;
				data_stream[(data_height + 1) * data_width + j] = 0;
			}

			for (i = 1; i < data_height + 1; i++) // copy the data from pgm
			{
				for (j = 0; j < data_width; j++)
				{
					data_stream[i * data_width + j] = my_data.data[i - 1][j];
				}
			}

			p = links.list; // send to children parts of the data
			toSend = 0;

			while (p != NULL)
			{
				p->index_of_start = toSend; // where data for certain children starts
				MPI_Send(&(data_stream[toSend]), (p->height + 2) * p->width, MPI_INT, p->info, tag, MPI_COMM_WORLD);
				toSend += p->height * p->width; // update starting point
				p = p->next;
			}
		}
		else
		{
			// receiving data
			MPI_Recv(data_stream, (data_height + 2) * data_width, MPI_INT, links.parent, MPI_ANY_TAG, MPI_COMM_WORLD, &status[rank]);
			tag = status[rank].MPI_TAG;

			// send data to children
			if (links.size > 0)
			{
				p = links.list;
				toSend = 0;

				while (p != NULL) {
					p->index_of_start = toSend;
					MPI_Send(&(data_stream[toSend]), (p->height + 2) * p->width, MPI_INT, p->info, tag, MPI_COMM_WORLD);
					toSend += p->height * p->width;
					p = p->next;
				}
			}
		}

		if (links.size == 0) // if the children is a worker
		{
			for (i = 1; i < data_height + 1; i++) // compute the new pixel after applying certain filter
			{
				for (j = 0; j < data_width; j++)
				{
					// construct pixel and their neighbors matrix (3 x 3 matrix)
					if (j == 0) // pixel in the first column
					{
						mat[0] = 0;
						mat[3] = 0;
						mat[6] = 0;
						mat[1] = data_stream[(i - 1) * data_width + j];
						mat[2] = data_stream[(i - 1) * data_width + j + 1];
						mat[4] = data_stream[i * data_width + j];
						mat[5] = data_stream[i * data_width + j + 1];
						mat[7] = data_stream[(i + 1) * data_width + j];
						mat[8] = data_stream[(i + 1) * data_width + j + 1];
					}
					else if (j == data_width - 1) // pixel in the last column
					{
						mat[2] = 0;
						mat[5] = 0;
						mat[8] = 0;
						mat[0] = data_stream[(i - 1) * data_width + j - 1];
						mat[1] = data_stream[(i - 1) * data_width + j];
						mat[3] = data_stream[i * data_width + j - 1];
						mat[4] = data_stream[i * data_width + j];
						mat[6] = data_stream[(i + 1) * data_width + j - 1];
						mat[7] = data_stream[(i + 1) * data_width + j];
					}
					else
					{ // the rest of the pixels
						mat[0] = data_stream[(i - 1) * data_width + j - 1];
						mat[1] = data_stream[(i - 1) * data_width + j];
						mat[2] = data_stream[(i - 1) * data_width + j + 1];
						mat[3] = data_stream[i * data_width + j - 1];
						mat[4] = data_stream[i * data_width + j];
						mat[5] = data_stream[i * data_width + j + 1];
						mat[6] = data_stream[(i + 1) * data_width + j - 1];
						mat[7] = data_stream[(i + 1) * data_width + j];
						mat[8] = data_stream[(i + 1) * data_width + j + 1];
					}

					if (tag == SOBEL_TAG) // check tag for filter application
					{
						data_computed[(i - 1) * data_width + j] = apply_sobel_filter(mat);
					}
					else
					{
						data_computed[(i - 1) * data_width + j] = apply_mean_removal_filter(mat);
					}
				}
			}
			statistics[rank] += data_height; // update statistics
			// send data to parent
			MPI_Send(data_computed, data_height * data_width, MPI_INT, links.parent, tag, MPI_COMM_WORLD);
		}
		else
		{
			// collect data from children and combines it
			p = links.list;
			while (p != NULL)
			{
				MPI_Recv(&(data_computed[p->index_of_start]), p->height * p->width, MPI_INT, p->info, MPI_ANY_TAG, MPI_COMM_WORLD, &status[rank]);
				p = p->next;
			}

			if (rank != 0) // all except the initiator send the data to their parents
			{
				MPI_Send(data_computed, data_height * data_width, MPI_INT, links.parent, tag, MPI_COMM_WORLD);
			}
		}

		if (rank == 0) // initiator combines data
		{
			for (i = 0; i < data_height; i++)
			{
				for (j = 0; j < data_width; j++)
				{
					my_data.data[i][j] = data_computed[i * data_width + j];
				}
			}

			write_to_pgm(file_out, &my_data); // write the new file
		}

		// frees allocated space
		free(data_stream);
		free(data_computed);
	}

	if (rank == 0)
	{
		fclose(fp); // close tasks file
	}

	// after the loop it means that the termination tag was sent, then the statistics must be created
	if (links.size == 0)
	{
		// worker children send statistics to parents
		MPI_Send(statistics, nProcesses, MPI_INT, links.parent, tag, MPI_COMM_WORLD);
	}
	else
	{
		// parents collects statistics from children and combines with what they already know
		p = links.list;
		while (p != NULL)
		{
			MPI_Recv(statistics_aux, nProcesses, MPI_INT, p->info, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			for (i = 0; i < nProcesses; i++)
			{
				if (statistics_aux[i] != 0)
				{
					statistics[i] = statistics_aux[i];
				}
			}

			TList* t = p;
			p = p->next;
			free(t); // free memory for children list
		}

		if (rank != 0) // all except the initiator send the statistics to their parents
		{
			MPI_Send(statistics, nProcesses, MPI_INT, links.parent, tag, MPI_COMM_WORLD);
		}
	}

	if (rank == 0) // initiator writes the statistics to the stats file
	{
		fp = fopen(argv[3], "wt");
		if (fp == NULL)
		{
			printf("File %s cannot be opened\n", argv[2]);
			exit(0);
		}

		for (i = 0; i < nProcesses; i++)
		{
			fprintf(fp, "%d: %d\n", i, statistics[i]);
		}

		fclose(fp);
	}

	// free allocated space for statistics
	free(statistics);
	free(statistics_aux);

	MPI_Finalize();
	return 0;
}