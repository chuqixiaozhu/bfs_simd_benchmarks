#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <omp.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <map>
//#define NUM_THREAD 4

using std::string;
using std::to_string;
using std::unordered_map;
using std::map;

typedef unordered_map<unsigned, unsigned[2]> hashmap_csr;

int	NUM_THREADS;
unsigned TILE_WIDTH;

double start;
double now;
FILE *time_out;
char *time_file = "timeline.txt";

//Structure to hold a node information
struct Node
{
	int starting;
	int num_of_edges;
};

void BFSGraph(int argc, char** argv);
void BFS_kernel(\
		hashmap_csr *tiles_indices,\
		int *h_graph_mask,\
		int *h_updating_graph_mask,\
		int *h_graph_visited,\
		int *h_graph_edges,\
		int *h_cost,\
		unsigned *tile_offsets,\
		unsigned num_of_nodes,\
		int edge_list_size,\
		int *is_empty_tile,\
		int *is_active_side,\
		int *is_updating_active_side,\
		unsigned side_length,\
		unsigned num_tiles\
		);

///////////////////////////////////////////////////////////////////////////////
// Main Program
///////////////////////////////////////////////////////////////////////////////
int main( int argc, char** argv) 
{
	start = omp_get_wtime();
	BFSGraph( argc, argv);
}

///////////////////////////////////////////////////////////////////////////////
// Apply BFS on a Graph
///////////////////////////////////////////////////////////////////////////////
void BFSGraph( int argc, char** argv) 
{
	unsigned int num_of_nodes = 0;
	int edge_list_size = 0;
	char *input_f;
	
	if(argc < 3){
		input_f = "/home/zpeng/benchmarks/data/pokec/soc-pokec";
		TILE_WIDTH = 1024;
	} else {
		input_f = argv[1];
		TILE_WIDTH = strtoul(argv[2], NULL, 0);
	}

	/////////////////////////////////////////////////////////////////////
	// Input real dataset
	/////////////////////////////////////////////////////////////////////
	//string prefix = string(input_f) + "_untiled";
	string prefix = string(input_f) + "_csr-tiled-" + to_string(TILE_WIDTH);
	string fname = prefix + "-0";
	FILE *fin = fopen(fname.c_str(), "r");
	fscanf(fin, "%u %u", &num_of_nodes, &edge_list_size);
	fclose(fin);
	unsigned side_length;
	if (num_of_nodes % TILE_WIDTH) {
		side_length = num_of_nodes / TILE_WIDTH + 1;
	} else {
		side_length = num_of_nodes / TILE_WIDTH;
	}
	unsigned num_tiles = side_length * side_length;
	// Read tile Offsets
	fname = prefix + "-tile_offsets";
	fin = fopen(fname.c_str(), "r");
	if (!fin) {
		fprintf(stderr, "cannot open file: %s\n", fname.c_str());
		exit(1);
	}
	unsigned *tile_offsets = (unsigned *) malloc(num_tiles * sizeof(unsigned));
	for (unsigned i = 0; i < num_tiles; ++i) {
		fscanf(fin, "%u", tile_offsets + i);
	}
	fclose(fin);
	//// Read file Offsets
	//fname = prefix + "-file_offsets";
	//fin = fopen(fname.c_str(), "r");
	//if (!fin) {
	//	fprintf(stderr, "cannot open file: %s\n", fname.c_str());
	//	exit(1);
	//}
	//NUM_THREADS = 64;
	//unsigned *file_offsets = (unsigned *) malloc(NUM_THREADS * sizeof(unsigned));
	//for (unsigned i = 0; i < NUM_THREADS; ++i) {
	//	fscanf(fin, "%u", file_offsets + i);
	//}
	//fclose(fin);

	hashmap_csr *tiles_indices = (hashmap_csr *) malloc(sizeof(hashmap_csr) * num_tiles);
	
	int *h_graph_edges = (int *) malloc(sizeof(int) * edge_list_size);
	//unsigned *n1s = (unsigned *) malloc(edge_list_size * sizeof(unsigned));
	//unsigned *n2s = (unsigned *) malloc(edge_list_size * sizeof(unsigned));
	int *is_empty_tile = (int *) malloc(sizeof(int) * num_tiles);
	memset(is_empty_tile, 0, sizeof(int) * num_tiles);
	NUM_THREADS = 64;
	//unsigned edge_bound = edge_list_size / NUM_THREADS;
	unsigned bound_tiles = num_tiles/NUM_THREADS;// number of tiles per file
#pragma omp parallel num_threads(NUM_THREADS) private(fname, fin)
{
	unsigned tid = omp_get_thread_num();
	//unsigned offset = tid * edge_bound;
	//unsigned file_offset = file_offsets[tid];
	fname = prefix + "-" + to_string(tid);
	fin = fopen(fname.c_str(), "r");
	if (!fin) {
		fprintf(stderr, "Error: cannot open file %s\n", fname.c_str());
		exit(1);
	}
	if (0 == tid) {
		fscanf(fin, "%u %u", &num_of_nodes, &edge_list_size);
	}
	unsigned offset_file = tid * bound_tiles;
	if (NUM_THREADS - 1 != tid) {
		for (unsigned i = 0; i < bound_tiles; ++i) {
			unsigned tile_id = i + offset_file;
			unsigned num_indices;
			unsigned num_edges;
			// Read number of indices, number of edges
			fscanf(fin, "%u %u", &num_indices, &num_edges);
			if (0 == num_indices) {
				is_empty_tile[tile_id] = 1;
				continue;
			}
			// Read indices
			for (unsigned i_indices = 0; i_indices < num_indices; ++i_indices) {
				unsigned index;
				unsigned start;
				unsigned outdegree;
				fscanf(fin, "%u %u %u", &index, &start, &outdegree);
				index--;
				start += tile_offsets[tile_id];
				//tiles_indices[tile_id][index] = {start, outdegree};
				tiles_indices[tile_id][index][0] = start;
				tiles_indices[tile_id][index][1] = outdegree;
			}
			// Read edges
			for (unsigned i_edges = 0; i_edges < num_edges; ++i_edges) {
				unsigned index = i_edges + tile_offsets[tile_id];
				fscanf(fin, "%d", h_graph_edges + index);
			}
		}
	} else { // the last file contains maybe more tiles
		for (unsigned i = 0; i + offset_file < num_tiles; ++i) {
			unsigned tile_id = i + offset_file;
			unsigned num_indices;
			unsigned num_edges;
			// Read number of indices, number of edges
			fscanf(fin, "%u %u", &num_indices, &num_edges);
			if (0 == num_indices) {
				is_empty_tile[tile_id] = 1;
				continue;
			}
			// Read indices
			for (unsigned i_indices = 0; i_indices < num_indices; ++i_indices) {
				unsigned index;
				unsigned start;
				unsigned outdegree;
				fscanf(fin, "%u %u %u", &index, &start, &outdegree);
				index--;
				start += tile_offsets[tile_id];
				//tiles_indices[tile_id][index] = {start, outdegree};
				tiles_indices[tile_id][index][0] = start;
				tiles_indices[tile_id][index][1] = outdegree;
			}
			// Read edges
			for (unsigned i_edges = 0; i_edges < num_edges; ++i_edges) {
				unsigned index = i_edges + tile_offsets[tile_id];
				fscanf(fin, "%d", h_graph_edges + index);
			}
		}
	}
}
	// Read nneibor
	//fname = prefix + "-nneibor";
	//fin = fopen(fname.c_str(), "r");
	//if (!fin) {
	//	fprintf(stderr, "Error: cannot open file %s\n", fname.c_str());
	//	exit(1);
	//}
	//unsigned *nneibor = (unsigned *) malloc(num_of_nodes * sizeof(unsigned));
	//for (unsigned i = 0; i < num_of_nodes; ++i) {
	//	fscanf(fin, "%u", nneibor + i);
	//}
	// End Input real dataset
	/////////////////////////////////////////////////////////////////////

	//unsigned NUM_CORE = 64;
	//omp_set_num_threads(NUM_CORE);
	//string file_prefix = input_f;
	//string file_name = file_prefix + "-v0.txt";
	//FILE *finput = fopen(file_name.c_str(), "r");
	//fscanf(finput, "%u", &num_of_nodes);
	//fclose(finput);
	//unsigned num_lines = num_of_nodes / NUM_CORE;
	//Node* h_graph_nodes = (Node*) malloc(sizeof(Node)*num_of_nodes);
	int *h_graph_mask = (int*) malloc(sizeof(int)*num_of_nodes);
	int *h_updating_graph_mask = (int*) malloc(sizeof(int)*num_of_nodes);
	int *h_graph_visited = (int*) malloc(sizeof(int)*num_of_nodes);
	int* h_cost = (int*) malloc(sizeof(int)*num_of_nodes);
	int *is_active_side = (int *) malloc(sizeof(int) * side_length);
	int *is_updating_active_side = (int *) malloc(sizeof(int) * side_length);
	unsigned source = 0;

	now = omp_get_wtime();
	time_out = fopen(time_file, "w");
	fprintf(time_out, "input end: %lf\n", now - start);
#ifdef ONEDEBUG
	printf("Input finished: %s\n", input_f);
#endif
	// BFS
	//for (unsigned i = 0; i < 9; ++i) {
	for (unsigned i = 0; i < 1; ++i) {
		NUM_THREADS = (unsigned) pow(2, i);
#ifndef ONEDEBUG
		sleep(10);
#endif
		// Re-initializing
		memset(h_graph_mask, 0, sizeof(int)*num_of_nodes);
		h_graph_mask[source] = 1;
		memset(h_updating_graph_mask, 0, sizeof(int)*num_of_nodes);
		memset(h_graph_visited, 0, sizeof(int)*num_of_nodes);
		h_graph_visited[source] = 1;
		for (unsigned i = 0; i < num_of_nodes; ++i) {
			h_cost[i] = -1;
		}
		h_cost[source] = 0;
		memset(is_active_side, 0, sizeof(int) * side_length);
		is_active_side[0] = 1;
		memset(is_updating_active_side, 0, sizeof(int) * side_length);

		BFS_kernel(\
				tiles_indices,\
				h_graph_mask,\
				h_updating_graph_mask,\
				h_graph_visited,\
				h_graph_edges,\
				h_cost,\
				tile_offsets,\
				num_of_nodes,\
				edge_list_size,\
				is_empty_tile,\
				is_active_side,\
				is_updating_active_side,\
				side_length,\
				num_tiles\
				);
		now = omp_get_wtime();
		fprintf(time_out, "Thread %u end: %lf\n", NUM_THREADS, now - start);
#ifdef ONEDEBUG
		printf("Thread %u finished.\n", NUM_THREADS);
#endif
	}
	fclose(time_out);

	//Store the result into a file

#ifdef ONEDEBUG
	NUM_THREADS = 64;
	omp_set_num_threads(NUM_THREADS);
	unsigned num_lines = num_of_nodes / NUM_THREADS;
#pragma omp parallel
{
	unsigned tid = omp_get_thread_num();
	unsigned offset = tid * num_lines;
	string file_prefix = "path/path";
	string file_name = file_prefix + to_string(tid) + ".txt";
	FILE *fpo = fopen(file_name.c_str(), "w");
	for (unsigned i = 0; i < num_lines; ++i) {
		unsigned index = i + offset;
		fprintf(fpo, "%d) cost:%d\n", index, h_cost[index]);
	}
	fclose(fpo);
}
#endif
	//printf("Result stored in result.txt\n");

	// cleanup memory
	//free(nneibor);
	//free(n1s);
	//free(n2s);
	//free( h_graph_nodes);
	free( h_graph_edges);
	free( h_graph_mask);
	free( h_updating_graph_mask);
	free( h_graph_visited);
	free( h_cost);
	free( tile_offsets);
	free( tiles_indices);
	free( is_empty_tile);
	free( is_active_side);
	free( is_updating_active_side);
	//free( file_offsets);
}

//void BFS_kernel(\
//		Node *h_graph_nodes,\
//		int *h_graph_mask,\
//		int *h_updating_graph_mask,\
//		int *h_graph_visited,\
//		int *h_graph_edges,\
//		int *h_cost,\
//		unsigned *offsets,\
//		unsigned num_of_nodes,\
//		int edge_list_size\
//		)
void BFS_kernel(\
		hashmap_csr *tiles_indices,\
		int *h_graph_mask,\
		int *h_updating_graph_mask,\
		int *h_graph_visited,\
		int *h_graph_edges,\
		int *h_cost,\
		unsigned *tile_offsets,\
		unsigned num_of_nodes,\
		int edge_list_size,\
		int *is_empty_tile,\
		int *is_active_side,\
		int *is_updating_active_side,\
		unsigned side_length,\
		unsigned num_tiles\
		)
{

	//printf("Start traversing the tree\n");
	omp_set_num_threads(NUM_THREADS);
	double start_time = omp_get_wtime();
	bool stop;
	do
	{
		//if no thread changes this value then the loop stops
		stop = true;

#pragma omp parallel for
		//for(unsigned int nid = 0; nid < num_of_nodes; nid++ )
		//{
		//	if (h_graph_mask[nid] == 1) {
		//		h_graph_mask[nid]=0;
		//		//int next_starting = h_graph_nodes[nid].starting + h_graph_nodes[nid].num_of_edges;
		//		//for(int i = h_graph_nodes[nid].starting; \
		//		//		i < next_starting; \
		//		//		i++)
		//		//{
		//		//	int id = h_graph_edges[i];
		//		//	if(!h_graph_visited[id])
		//		//	{
		//		//		h_cost[id]=h_cost[nid]+1;
		//		//		h_updating_graph_mask[id]=1;
		//		//	}
		//		//}
		//	}
		//}
		for (unsigned side_id = 0; side_id < side_length; ++side_id) {
			if (!is_active_side[side_id]) {
				continue;
			}
			is_active_side[side_id] = 0;
			for (unsigned i = 0; i < side_length; ++i) {
				unsigned tile_id = side_id * side_length + i;
				for (auto itor = tiles_indices[tile_id].begin(); \
						itor != tiles_indices[tile_id].end(); \
						++itor) {
					unsigned vertex_id = itor->first;
					if (0 == h_graph_mask[vertex_id]) {
						continue;
					}
					h_graph_mask[vertex_id] = 0;
					unsigned start = itor->second[0];
					unsigned outdegree = itor->second[1];
					for (unsigned j = 0; j < outdegree; ++j) {
						unsigned index = start + j;
						unsigned end = h_graph_edges[index];
						if (!h_graph_visited[end]) {
							h_cost[end] = h_cost[vertex_id];
							h_updating_graph_mask[end] = 1;
							is_updating_active_side[end/TILE_WIDTH] = 1;
						}
					}
				}
			}
		}
#pragma omp parallel for
		//for(unsigned int nid=0; nid< num_of_nodes ; nid++ )
		//{
		//	if (h_updating_graph_mask[nid] == 1) {
		//		h_graph_mask[nid]=1;
		//		h_graph_visited[nid]=1;
		//		stop = false;
		//		h_updating_graph_mask[nid]=0;
		//	}
		//}
		for (unsigned side_id = 0; side_id < side_length; ++side_id) {
			if (!is_updating_active_side[side_id]) {
				continue;
			}
			is_updating_active_side[side_id] = 0;
			is_active_side[side_id] = 1;
			stop = false;
			for (unsigned i = 0; i < TILE_WIDTH; ++i) {
				unsigned vertex_id = i + side_id * TILE_WIDTH;
				if (0 == h_updating_graph_mask[vertex_id]) {
					continue;
				}
				h_updating_graph_mask[vertex_id] = 0;
				h_graph_mask[vertex_id] = 1;
				h_graph_visited[vertex_id] = 1;
			}
		}
	}
	while(!stop);
	double end_time = omp_get_wtime();
	printf("%d %lf\n", NUM_THREADS, (end_time - start_time));
}
