#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <omp.h>
#include <immintrin.h>
using std::ifstream;
using std::string;
using std::getline;
using std::stringstream;
using std::cout;
using std::endl;
using std::vector;
using std::to_string;

#define DUMP 0.85
//#define MAX_NODES 1700000
//#define MAX_EDGES 40000000
#define MAX_NODES 67108864
#define MAX_EDGES 2147483648

#define NUM_P_INT 16 // Number of packed intergers in one __m512i variable
#define ALIGNED_BYTES 64

//struct Graph {
//	//int n1[MAX_EDGES];
//	//int n2[MAX_EDGES];
//	int nneibor[MAX_NODES];
//};
//Graph grah;

unsigned nnodes, nedges;
//float rank[MAX_NODES];
//float sum[MAX_NODES];
unsigned TILE_WIDTH;


//void page_rank(unsigned *tops, unsigned *offsets, unsigned num_tiles);
//void print();

//////////////////////////////////////////////////////////////////////////////////
// Commented for clone
//void input(char filename[]) {
//	FILE *fin = fopen(filename, "r");
//	if (!fin) {
//		fprintf(stderr, "cannot open file: %s\n", filename);
//		exit(1);
//	}
//
//	fscanf(fin, "%u %u", &nnodes, &nedges);
//	memset(nneibor, 0, sizeof(nneibor));
//	unsigned num_tiles;
//	//unsigned long long num_tiles;
//	unsigned side_length;
//	if (nnodes % TILE_WIDTH) {
//		side_length = nnodes / TILE_WIDTH + 1;
//	} else {
//		side_length = nnodes / TILE_WIDTH;
//	}
//	num_tiles = side_length * side_length;
//	if (nedges < num_tiles) {
//		fprintf(stderr, "Error: tile size is too small.\n");
//		exit(2);
//	}
//	//unsigned max_top = nedges / num_tiles * 16;
//	unsigned max_top = TILE_WIDTH * TILE_WIDTH / 128;
//	unsigned **tiles_n1 = (unsigned **) _mm_malloc(num_tiles * sizeof(unsigned *), ALIGNED_BYTES);
//	unsigned **tiles_n2 = (unsigned **) _mm_malloc(num_tiles * sizeof(unsigned *), ALIGNED_BYTES);
//	for (unsigned i = 0; i < num_tiles; ++i) {
//		tiles_n1[i] = (unsigned *) _mm_malloc(max_top * sizeof(unsigned), ALIGNED_BYTES);
//		tiles_n2[i] = (unsigned *) _mm_malloc(max_top * sizeof(unsigned), ALIGNED_BYTES);
//	}
//	unsigned *tops = (unsigned *) _mm_malloc(num_tiles * sizeof(unsigned), ALIGNED_BYTES);
//	memset(tops, 0, num_tiles * sizeof(unsigned));
//	for (unsigned i = 0; i < nedges; ++i) {
//		unsigned n1;
//		unsigned n2;
//		fscanf(fin, "%u %u", &n1, &n2);
//		n1--;
//		n2--;
//		unsigned n1_id = n1 / TILE_WIDTH;
//		unsigned n2_id = n2 / TILE_WIDTH;
//		//unsigned n1_id = n1 % side_length;
//		//unsigned n2_id = n2 % side_length;
//		unsigned tile_id = n1_id * side_length + n2_id;
//
//		unsigned *top = tops + tile_id;
//		if (*top == max_top) {
//			fprintf(stderr, "Error: the tile %u is full.\n", tile_id);
//			exit(1);
//		}
//		tiles_n1[tile_id][*top] = n1;
//		tiles_n2[tile_id][*top] = n2;
//		(*top)++;
//		nneibor[n1]++;
//	}
//	fclose(fin);
//
//	// PageRank
//	for (unsigned i = 0; i < 9; ++i) {
//		NUM_THREADS = (unsigned) pow(2, i);
//		page_rank(tiles_n1, tiles_n2, tops, num_tiles);
//	}
//
//	// Free memory
//	for (unsigned i = 0; i < num_tiles; ++i) {
//		_mm_free(tiles_n1[i]);
//		_mm_free(tiles_n2[i]);
//	}
//	_mm_free(tiles_n1);
//	_mm_free(tiles_n2);
//	_mm_free(tops);
//}
////////////////////////////////////////////////////////////////////////////

void manual_sort(vector<unsigned> &n1v, vector<unsigned> &n2v)
{
	unsigned length = n1v.size();
	for (unsigned i = length; i > 0; --i) {
		int swapped = 0;
		for (unsigned j = 0; j < i - 1; ++j) {
			if (n1v[j] > n1v[j+1]) {
				unsigned tmp = n1v[j];
				n1v[j] = n1v[j+1];
				n1v[j+1] = tmp;
				tmp = n2v[j];
				n2v[j] = n2v[j+1];
				n2v[j+1] = tmp;
				swapped = 1;
			}
		}
		if (!swapped) {
			break;
		}
	}
}

void input(char filename[]) {
#ifdef ONEDEBUG
	printf("input: %s\n", filename);
#endif
	FILE *fin = fopen(filename, "r");
	if (!fin) {
		fprintf(stderr, "cannot open file: %s\n", filename);
		exit(1);
	}

	fscanf(fin, "%u %u", &nnodes, &nedges);
	unsigned *nneibor = (unsigned *) malloc(nnodes * sizeof(unsigned));
	memset(nneibor, 0, nnodes * sizeof(unsigned));
	unsigned num_tiles;
	unsigned side_length;
	if (nnodes % TILE_WIDTH) {
		side_length = nnodes / TILE_WIDTH + 1;
	} else {
		side_length = nnodes / TILE_WIDTH;
	}
	num_tiles = side_length * side_length;
	if (nedges/num_tiles < NUM_P_INT/2) {
		printf("nedges: %u, num_tiles: %u, average: %u\n", nedges, num_tiles, nedges/num_tiles);
		fprintf(stderr, "Error: the tile width %u is too small.\n", TILE_WIDTH);
		exit(2);
	}
	vector< vector<unsigned> > tiles_n1v;
	tiles_n1v.resize(num_tiles);
	vector< vector<unsigned> > tiles_n2v;
	tiles_n2v.resize(num_tiles);
	for (unsigned i = 0; i < nedges; ++i) {
		unsigned n1;
		unsigned n2;
		fscanf(fin, "%u %u", &n1, &n2);
		n1--;
		n2--;
		unsigned n1_id = n1 / TILE_WIDTH;
		unsigned n2_id = n2 / TILE_WIDTH;
		unsigned tile_id = n1_id * side_length + n2_id;
		nneibor[n1]++;
		n1++;
		n2++;
		tiles_n1v[tile_id].push_back(n1);
		tiles_n2v[tile_id].push_back(n2);
	}
	// test for non-zero tils
	//unsigned non_zero = 0;
	//for (unsigned i = 0; i < num_tiles; ++i) {
	//	if (tiles_n1v[i].size()) {
	//		non_zero++;
	//	}
	//}
	//printf("non empty: %u\n", non_zero);
	//exit(0);
	// End test
	for (unsigned i = 0; i < num_tiles; ++i) {
		manual_sort(tiles_n1v[i], tiles_n2v[i]);
	}

	// OOC -> CSR
	string prefix = string(filename) + "_csr-tiled-" + to_string(TILE_WIDTH);
	vector< vector<unsigned> > tiles_indices;
	tiles_indices.resize(num_tiles);
	vector< vector<unsigned> > tiles_startings;
	tiles_startings.resize(num_tiles);
	vector< vector<unsigned> > tiles_outdegrees;
	tiles_outdegrees.resize(num_tiles);
	unsigned NUM_THREADS = 64;
#pragma omp parallel for num_threads(NUM_THREADS)
	for (unsigned tile_id = 0; tile_id < num_tiles; ++tile_id) {
		unsigned tile_rowid = tile_id / side_length;
		unsigned tile_colid = tile_id % side_length;
		unsigned n1_offset = tile_rowid * TILE_WIDTH;
		unsigned size_tile = tiles_n1v[tile_id].size();
		// Get Startings
		unsigned *n1_counts = (unsigned *) malloc(sizeof(unsigned) * TILE_WIDTH);
		memset(n1_counts, 0, sizeof(unsigned) * TILE_WIDTH);
		for (unsigned j = 0; j < size_tile; ++j) {
			unsigned n1 = tiles_n1v[tile_id][j];
			//unsigned n2 = tiles_n2v[tile_id][j];
			n1--;
			n1_counts[n1 - n1_offset]++;
		}
		unsigned *startings = (unsigned *) malloc(sizeof(unsigned) * TILE_WIDTH);
		memset(startings, 0, sizeof(unsigned) * TILE_WIDTH);
		unsigned start = 0;
		for (unsigned k = 0; k < TILE_WIDTH; ++k) {
			startings[k] = start;
			start += n1_counts[k];
		}

		// Save startings as indices
		for (unsigned k = 0; k < TILE_WIDTH; ++k) {
			if (k != TILE_WIDTH - 1) {
				//fprintf(fout, "%u %u\n", startings[k], startings[k+1] - startings[k]);
				if (startings[k] != startings[k+1]) {
					tiles_indices[tile_id].push_back(k + n1_offset + 1);
					tiles_startings[tile_id].push_back(startings[k]);
					tiles_outdegrees[tile_id].push_back(n1_counts[k]);
				}
			} else {
				//fprintf(fout, "%u %u\n", startings[k], size_tile - startings[k]);
				if (startings[k] != size_tile) {
					tiles_indices[tile_id].push_back(k + n1_offset + 1);
					tiles_startings[tile_id].push_back(startings[k]);
					tiles_outdegrees[tile_id].push_back(n1_counts[k]);
				}
			}
		}
		
		////////////////////////////////////////////////
		// Need modification
		////////////////////////////////////////////////
		//// Write Startings to file
		//string fname = prefix + "-" + to_string(tile_id);
		//FILE *fout = fopen(fname.c_str(), "w");
		//if (0 == tile_id) {
		//	fprintf(fout, "%u %u\n", nnodes, nedges);
		//}
		//fprintf(fout, "%u %u\n\n", n1_offset, size_tile); // Head index offset, size of tile
		//for (unsigned k = 0; k < TILE_WIDTH; ++k) {
		//	if (k != TILE_WIDTH - 1) {
		//		fprintf(fout, "%u %u\n", startings[k], startings[k+1] - startings[k]);
		//	} else {
		//		fprintf(fout, "%u %u\n", startings[k], size_tile - startings[k]);
		//	}
		//}
		//fprintf(fout, "\n");

		//// Write End nodes to file
		//for (unsigned k = 0; k < size_tile; ++k) {
#ifdef O//NEDEBUG
		//	fprintf(fout, "%u %u\n", tiles_n1v[tile_id][k], tiles_n2v[tile_id][k]);//test
#else
		//	fprintf(fout, "%u\n", tiles_n2v[tile_id][k]);
#endif
		//}
		////////////////////////////////////////////////

		//fclose(fout);
		free(n1_counts);
		free(startings);
	}

	// Write to files
	NUM_THREADS = 64;
	unsigned *files_edges_counts = (unsigned *) malloc(sizeof(unsigned) * NUM_THREADS);
	memset(files_edges_counts, 0, sizeof(unsigned) * NUM_THREADS);
	unsigned bound_tiles = num_tiles/NUM_THREADS;// number of tiles per file
#pragma omp parallel num_threads(NUM_THREADS)
{
	unsigned tid = omp_get_thread_num();
	string fname = prefix + "-" + to_string(tid);
	FILE *fout = fopen(fname.c_str(), "w");
	if (0 == tid) {
		fprintf(fout, "%u %u\n\n", nnodes, nedges);
	}
	unsigned offset_file = tid * bound_tiles;
	if (NUM_THREADS - 1 != tid) {
		for (unsigned i = 0; i < bound_tiles; ++i) {
			unsigned tile_id = i + offset_file;
			unsigned num_indices = tiles_indices[tile_id].size();
			unsigned num_edges = tiles_n1v[tile_id].size();
			// Write num of indices and edges of this tile
			fprintf(fout, "%u %u\n\n", num_indices, num_edges);
			// Write indices
			for (unsigned j = 0; j < num_indices; ++j) {
				fprintf(fout, "%u %u %u\n", tiles_indices[tile_id][j], tiles_startings[tile_id][j], tiles_outdegrees[tile_id][j]);
			}
			fprintf(fout, "\n");
			// Write ends
			for (unsigned j = 0; j < num_edges; ++j) {
				fprintf(fout, "%u\n", tiles_n2v[tile_id][j]);
			}
			fprintf(fout, "\n");
			files_edges_counts[tid] += num_edges;
		}
	} else { // the last file contains maybe more tiles
		for (unsigned i = 0; i + offset_file < num_tiles; ++i) {
			unsigned tile_id = i + offset_file;
			unsigned num_indices = tiles_indices[tile_id].size();
			unsigned num_edges = tiles_n1v[tile_id].size();
			// Write num of indices and edges of this tile
			fprintf(fout, "%u %u\n\n", num_indices, num_edges);
			// Write indices
			for (unsigned j = 0; j < num_indices; ++j) {
				fprintf(fout, "%u %u %u\n", tiles_indices[tile_id][j], tiles_startings[tile_id][j], tiles_outdegrees[tile_id][j]);
			}
			fprintf(fout, "\n");
			// Write ends
			for (unsigned j = 0; j < num_edges; ++j) {
				fprintf(fout, "%u\n", tiles_n2v[tile_id][j]);
			}
			fprintf(fout, "\n");
			files_edges_counts[tid] += num_edges;
		}
	}
}


	//unsigned *tiles_n1 = (unsigned *) malloc(nedges * sizeof(unsigned));
	//unsigned *tiles_n2 = (unsigned *) malloc(nedges * sizeof(unsigned));
	//unsigned tile_index = 0;
	//for (unsigned i = 0; i < num_tiles; ++i) {
	//	unsigned bound = tiles_n1v[i].size();
	//	for (unsigned j = 0; j < bound; ++j) {
	//		tiles_n1[tile_index] = tiles_n1v[i][j];
	//		tiles_n2[tile_index] = tiles_n2v[i][j];
	//		++tile_index;
	//	}
	//}

	//string prefix = string(filename) + "_tiled-" + to_string(TILE_WIDTH);
//	unsigned edge_bound = nedges / ALIGNED_BYTES;
//	unsigned NUM_THREADS = 64;
//#pragma omp parallel num_threads(NUM_THREADS)
//{
//	unsigned tid = omp_get_thread_num();
//	unsigned offset = tid * edge_bound;
//	string fname = prefix + "-" + to_string(tid);
//	FILE *fout = fopen(fname.c_str(), "w");
//	if (0 == tid) {
//		fprintf(fout, "%u %u\n", nnodes, nedges);
//	}
//	if (NUM_THREADS - 1 != tid) {
//		for (unsigned i = 0; i < edge_bound; ++i) {
//			unsigned index = i + offset;
//			fprintf(fout, "%u %u\n", tiles_n1[index], tiles_n2[index]);
//		}
//	} else {
//		for (unsigned i = 0; i + offset < nedges; ++i) {
//			unsigned index = i + offset;
//			fprintf(fout, "%u %u\n", tiles_n1[index], tiles_n2[index]);
//		}
//	}
//	fclose(fout);
//}
	// Write tile offsets
	string fname = prefix + "-tile_offsets";
	FILE *fout = fopen(fname.c_str(), "w");
	unsigned offset = 0;
	for (unsigned i = 0; i < num_tiles; ++i) {
		unsigned size = tiles_n1v[i].size();
		fprintf(fout, "%u\n", offset);//Format: offset
		offset += size;
	}
	fclose(fout);

	//// Write file offsets
	//fname = prefix + "-file_offsets";
	//fout = fopen(fname.c_str(), "w");
	//offset = 0;
	//NUM_THREADS = 64;
	//for (unsigned i = 0; i < NUM_THREADS; ++i) {
	//	fprintf(fout, "%u\n", offset);//Format: offset
	//	offset += files_edges_counts[i];
	//}
	//fclose(fout);

	// Write number of neighbors
	fname = prefix + "-nneibor";
	fout = fopen(fname.c_str(), "w");
	for (unsigned i = 0; i < nnodes; ++i) {
		fprintf(fout, "%u\n", nneibor[i]);
	}
	// Clean the vectors for saving memory
	fclose(fin);
	fclose(fout);
	free(nneibor);
	free(files_edges_counts);
}

void input_untiled(char filename[]) {
#ifdef ONEDEBUG
	printf("input: %s\n", filename);
#endif
	FILE *fin = fopen(filename, "r");
	if (!fin) {
		fprintf(stderr, "cannot open file: %s\n", filename);
		exit(1);
	}

	fscanf(fin, "%u %u", &nnodes, &nedges);
	unsigned *nneibor = (unsigned *) malloc(nnodes * sizeof(unsigned));
	memset(nneibor, 0, nnodes * sizeof(unsigned));
	//unsigned num_tiles;
	//unsigned side_length;
	//if (nnodes % TILE_WIDTH) {
	//	side_length = nnodes / TILE_WIDTH + 1;
	//} else {
	//	side_length = nnodes / TILE_WIDTH;
	//}
	//num_tiles = side_length * side_length;
	//if (nedges/num_tiles < NUM_P_INT/2) {
	//	printf("nedges: %u, num_tiles: %u, average: %u\n", nedges, num_tiles, nedges/num_tiles);
	//	fprintf(stderr, "Error: the tile width %u is too small.\n", TILE_WIDTH);
	//	exit(2);
	//}
	//vector< vector<unsigned> > tiles_n1v;
	//tiles_n1v.resize(num_tiles);
	//vector< vector<unsigned> > tiles_n2v;
	//tiles_n2v.resize(num_tiles);
	unsigned *n1s = (unsigned *) malloc(nedges * sizeof(unsigned));
	unsigned *n2s = (unsigned *) malloc(nedges * sizeof(unsigned));
	for (unsigned i = 0; i < nedges; ++i) {
		//fscanf(fin, "%u %u", n1s + i, n2s + i);
		unsigned n1;
		unsigned n2;
		fscanf(fin, "%u %u", &n1, &n2);
		n1s[i] = n1;
		n2s[i] = n2;
		n1--;
		n2--;
		nneibor[n1]++;
		//unsigned n1_id = n1 / TILE_WIDTH;
		//unsigned n2_id = n2 / TILE_WIDTH;
		//unsigned tile_id = n1_id * side_length + n2_id;
		//nneibor[n1]++;
		//n1++;
		//n2++;
		//tiles_n1v[tile_id].push_back(n1);
		//tiles_n2v[tile_id].push_back(n2);
	}
	//unsigned *tiles_n1 = (unsigned *) malloc(nedges * sizeof(unsigned));
	//unsigned *tiles_n2 = (unsigned *) malloc(nedges * sizeof(unsigned));
	//unsigned tile_index = 0;
	//for (unsigned i = 0; i < num_tiles; ++i) {
	//	unsigned bound = tiles_n1v[i].size();
	//	for (unsigned j = 0; j < bound; ++j) {
	//		tiles_n1[tile_index] = tiles_n1v[i][j];
	//		tiles_n2[tile_index] = tiles_n2v[i][j];
	//		++tile_index;
	//	}
	//}

	//string prefix = string(filename) + "_tiled-" + to_string(TILE_WIDTH);
	string prefix = string(filename) + "_untiled";
	unsigned NUM_THREADS = 64;
	unsigned edge_bound = nedges / NUM_THREADS;
#pragma omp parallel num_threads(NUM_THREADS)
{
	unsigned tid = omp_get_thread_num();
	unsigned offset = tid * edge_bound;
	string fname = prefix + "-" + to_string(tid);
	FILE *fout = fopen(fname.c_str(), "w");
	if (0 == tid) {
		fprintf(fout, "%u %u\n", nnodes, nedges);
	}
	if (NUM_THREADS - 1 != tid) {
		for (unsigned i = 0; i < edge_bound; ++i) {
			unsigned index = i + offset;
			fprintf(fout, "%u %u\n", n1s[index], n2s[index]);
		}
	} else {
		for (unsigned i = 0; i + offset < nedges; ++i) {
			unsigned index = i + offset;
			fprintf(fout, "%u %u\n", n1s[index], n2s[index]);
		}
	}
	fclose(fout);
}
	//string fname = prefix + "-offsets";
	//FILE *fout = fopen(fname.c_str(), "w");
	//unsigned offset = 0;
	//for (unsigned i = 0; i < num_tiles; ++i) {
	//	unsigned size = tiles_n1v[i].size();
	//	fprintf(fout, "%u\n", offset);//Format: offset
	//	offset += size;
	//}
	//fclose(fout);
	string fname = prefix + "-nneibor";
	FILE *fout = fopen(fname.c_str(), "w");
	for (unsigned i = 0; i < nnodes; ++i) {
		fprintf(fout, "%u\n", nneibor[i]);
	}
	// Clean the vectors for saving memory
	fclose(fin);
	fclose(fout);
	free(nneibor);
	free(n1s);
	free(n2s);
}

int main(int argc, char *argv[]) {
	char *filename;
	if (argc > 2) {
		filename = argv[1];
		TILE_WIDTH = strtoul(argv[2], NULL, 0);
	} else {
		filename = "/home/zpeng/benchmarks/data/pokec/soc-pokec";
		TILE_WIDTH = 1024;
	}
#ifdef UNTILE
	input_untiled(filename);
#else
	input(filename);
#endif
	return 0;
}
