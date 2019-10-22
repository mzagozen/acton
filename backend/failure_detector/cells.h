/*
 * cells.h
 *
 *      Author: aagapi
 */

#include "db_messages.pb-c.h"
#include "vector_clock.h"

#ifndef BACKEND_FAILURE_DETECTOR_CELLS_H_
#define BACKEND_FAILURE_DETECTOR_CELLS_H_

typedef struct cell_address
{
	long table_key;
	long * keys;
	int no_keys;
} cell_address;

cell_address * init_cell_address(long table_key, long * keys, int no_keys);
cell_address * init_cell_address_copy(long table_key, long * keys, int no_keys);
cell_address * init_cell_address_copy2(long table_key, long * primary_keys, int no_primary_keys, long * clustering_keys, int no_clustering_keys);
void free_cell_address(cell_address * ca);
void init_cell_address_msg(CellAddressMessage * msg, cell_address * ca);
cell_address * init_cell_address_from_msg(CellAddressMessage * msg);
void free_cell_address_msg(CellAddressMessage * msg);
int serialize_cell_address(cell_address * ca, void ** buf, unsigned * len);
int deserialize_cell_address(void * buf, unsigned msg_len, cell_address ** ca);
int equals_cell_address(cell_address * ca1, cell_address * ca2);
char * to_string_cell_address(cell_address * ca, char * msg_buff);

typedef struct cell
{
	long table_key;
	long * keys;
	int no_keys;
	long * columns;
	int no_columns;
	vector_clock * version;
} cell;

cell * init_cell(long table_key, long * keys, int no_keys, long * columns, int no_columns, vector_clock * version);
cell * init_cell_copy(long table_key, long * keys, int no_keys, long * columns, int no_columns, vector_clock * version);
void copy_cell(cell * ca, long table_key, long * keys, int no_keys, long * columns, int no_columns, vector_clock * version);
cell_address * get_cell_address(cell * c);
cell * init_cell_from_msg(VersionedCellMessage * msg);
cell * copy_cell_from_msg(cell * c, VersionedCellMessage * msg);
void free_cell(cell * ca);
void free_cell_ptrs(cell * ca);
void init_cell_msg(VersionedCellMessage * msg, cell * ca, VectorClockMessage * vc_msg);
void free_cell_msg(VersionedCellMessage * msg);
int serialize_cell(cell * ca, void ** buf, unsigned * len);
int deserialize_cell(void * buf, unsigned msg_len, cell ** ca);
int equals_cell(cell * ca1, cell * ca2);
char * to_string_cell(cell * ca, char * msg_buff);

int serialize_vc(vector_clock * vc, void ** buf, unsigned * len);
int deserialize_vc(void * buf, unsigned msg_len, vector_clock ** vc);

#endif /* BACKEND_FAILURE_DETECTOR_CELLS_H_ */
