/*
 * client.c
 *
 *      Author: aagapi
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <uuid/uuid.h>

#include "client_api.h"
#include "fastrand.h"

typedef struct actor_collection_item {
	int actor_id;
	int collection_id;
	int item_id;
	int item_value;
} actor_collection_item_t;

int no_cols = 4;
int no_primary_keys = 1;
int no_clustering_keys = 2;
int no_index_keys = 1;

int no_queue_cols = 2;
int no_enqueues = 5;

int no_actors = 2;
int no_collections = 2;
int no_items = 2;


db_schema_t * create_schema() {
	int primary_key_idx = 0;
	int clustering_key_idxs[2];
	clustering_key_idxs[0]=1;
	clustering_key_idxs[1]=2;
	int index_key_idx=3;

	int * col_types = (int *) malloc(no_cols * sizeof(int));

	for(int i=0;i<no_cols;i++)
		col_types[i] = DB_TYPE_INT32;

	db_schema_t* db_schema = db_create_schema(col_types, no_cols, &primary_key_idx, no_primary_keys, clustering_key_idxs, no_clustering_keys, &index_key_idx, no_index_keys);

	assert(db_schema != NULL && "Schema creation failed");

	return db_schema;
}

// Tests:

int populate_db(db_schema_t * schema, remote_db_t * db, unsigned int * fastrandstate)
{
	printf("TEST: populate_db\n");

	uuid_t txnid;
	uuid_generate(txnid);
	WORD * column_values = (WORD *) malloc(no_cols * sizeof(WORD));

	for(long aid=0;aid<no_actors;aid++)
	{
		for(long cid=0;cid<no_collections;cid++)
		{
			for(long iid=0;iid<no_items;iid++)
			{
				column_values[0] = (WORD) aid;
				column_values[1] = (WORD) cid;
				column_values[2] = (WORD) iid;
				column_values[3] = (WORD) iid + 1;

				if(remote_insert_in_txn(column_values, no_cols, (WORD) 0, schema, NULL, db) != 0) // &txnid
					return -1;
			}
		}
	}

	return 0;
}

int delete_test(db_schema_t * schema, remote_db_t * db, unsigned int * fastrandstate)
// Deletes row for last actor
{
	printf("TEST: delete_test\n");

	uuid_t txnid;
	uuid_generate(txnid);
	WORD row_key = (WORD) no_actors - 1;
	return remote_delete_row_in_txn(&row_key, 1, (WORD) 0, schema, NULL, db); //  &txnid
}

int test_search_pk(db_schema_t * schema, remote_db_t * db, unsigned int * fastrandstate)
{
	printf("TEST: test_search_pk\n");

	char print_buff[1024];
	uuid_t txnid;
	uuid_generate(txnid);

	for(long aid=0;aid<no_actors;aid++)
	{
		db_row_t * row = remote_search_in_txn((WORD *) &aid, 1, (WORD) 0, &txnid, db);

		print_long_row(row);

		if(row == NULL)
		{
			printf("Read back wrong NULL row for cell (%ld)!\n", aid);
			return -1;
		}

		if((long) row->key != aid)
		{
			printf("Read back mismatched pk %ld ( != %ld) in cell!\n", (long) row->key, aid);
			return -1;
		}
	}

	return 0;
}

int test_search_pk_ck1(db_schema_t * schema, remote_db_t * db, unsigned int * fastrandstate)
{
	printf("TEST: test_search_pk_ck1\n");

	char print_buff[1024];
	uuid_t txnid;
	uuid_generate(txnid);

	for(long aid=0;aid<no_actors;aid++)
	{
		for(long cid=0;cid<no_collections;cid++)
		{
			db_row_t * row = remote_search_clustering_in_txn((WORD *) &aid, (WORD *) &cid, 1, (WORD) 0, schema, &txnid, db);

			print_long_row(row);

			if(row == NULL)
			{
				printf("Read back wrong NULL row for cell (%ld, %ld)!\n", aid, cid);
				return -1;
			}

			if((long) row->key != cid)
			{
				printf("Read back mismatched ck1 %ld ( != %ld) in cell (%ld, %ld)!\n", (long) row->key, cid, aid, cid);
				return -1;
			}
		}
	}

	return 0;
}

int test_search_pk_ck1_ck2(db_schema_t * schema, remote_db_t * db, unsigned int * fastrandstate)
{
	printf("TEST: test_search_pk_ck1_ck2\n");

	char print_buff[1024];
	WORD * cks = (WORD *) malloc(2 * sizeof(WORD));

	uuid_t txnid;
	uuid_generate(txnid);

	for(long aid=0;aid<no_actors;aid++)
	{
		for(long cid=0;cid<no_collections;cid++)
		{
			for(long iid=0;iid<no_items;iid++)
			{
				cks[0] = (WORD) cid;
				cks[1] = (WORD) iid;

				db_row_t * row = remote_search_clustering_in_txn((WORD *) &aid, cks, 2, (WORD) 0, schema, &txnid, db);

				print_long_row(row);

				if((long) row->key != iid)
				{
					printf("Read back mismatched ck1 %ld ( != %ld) in cell (%ld, %ld, %ld)!\n", (long) row->key, iid, aid, cid, iid);
					return -1;
				}
			}
		}
	}

	return 0;
}

// Queue tests:

void consumer_callback(queue_callback_args * qca)
{
	printf("Consumer %ld/%ld/%ld received notification for queue %ld/%ld, status %d\n",
			(long) qca->app_id, (long) qca->shard_id, (long) qca->consumer_id,
			(long) qca->table_key, (long) qca->queue_id,
			qca->status);
}


int test_create_queue(remote_db_t * db)
{
	printf("TEST: create_queue\n");

	uuid_t txnid;
	uuid_generate(txnid);

	return remote_create_queue_in_txn((WORD) 1, (WORD) 1, NULL, db); // &txnid
}

int test_delete_queue(remote_db_t * db)
{
	printf("TEST: delete_queue\n");

	uuid_t txnid;
	uuid_generate(txnid);

	return remote_delete_queue_in_txn((WORD) 1, (WORD) 1, NULL, db); // &txnid
}

int test_subscribe_queue(remote_db_t * db)
{
	printf("TEST: subscribe_queue\n");
	long prev_read_head = -1, prev_consume_head = -1;
	queue_callback * qc = get_queue_callback(consumer_callback);

	return remote_subscribe_queue((WORD) 0, (WORD) 1, (WORD) 2, (WORD) 1, (WORD) 1, qc, &prev_read_head, &prev_consume_head, db); // &txnid
}

int test_unsubscribe_queue(remote_db_t * db)
{
	printf("TEST: unsubscribe_queue\n");
	return remote_unsubscribe_queue((WORD) 0, (WORD) 1, (WORD) 2, (WORD) 1, (WORD) 1, db); // &txnid
}

int test_enqueue(remote_db_t * db)
{
	printf("TEST: enqueue\n");

	uuid_t txnid;
	uuid_generate(txnid);
	WORD * column_values = (WORD *) malloc(no_queue_cols * sizeof(WORD));

	for(long i=0;i<no_enqueues;i++)
	{
		column_values[0] = (WORD) i;
		column_values[1] = (WORD) i + 1;

		if(remote_enqueue_in_txn(column_values, no_queue_cols, (WORD) 1, (WORD) 1, NULL, db) != 0) // &txnid
			return -1;
	}

	return 0;
}

int test_read_queue(remote_db_t * db)
{
	printf("TEST: read_queue\n");
	int max_entries = no_enqueues;
	int entries_read;
	long new_read_head;
	snode_t* start_row, * end_row;
	int ret = remote_read_queue_in_txn((WORD) 0, (WORD) 1, (WORD) 2, (WORD) 1, (WORD) 1,
									max_entries, &entries_read, &new_read_head,
									&start_row, &end_row, NULL, db); // &txnid

	assert(ret == QUEUE_STATUS_READ_COMPLETE);
	assert(entries_read == no_enqueues);
	assert(new_read_head == no_enqueues - 1);
	assert(end_row->key - start_row->key == (entries_read - 1));

	return ret;
}

int test_consume_queue(remote_db_t * db)
{
	printf("TEST: consume_queue\n");
	return (remote_consume_queue_in_txn((WORD) 0, (WORD) 1, (WORD) 2, (WORD) 1, (WORD) 1, no_enqueues - 1, NULL, db) == (no_enqueues - 1)); // &txnid
}

int main(int argc, char **argv) {
    int portno, n, status;
    char *hostname;
    unsigned int seed;

    GET_RANDSEED(&seed, 0); // thread_id

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    remote_db_t * db = get_remote_db(1);

    add_server_to_membership(hostname, portno, db, &seed);

    db_schema_t * schema = create_schema();

    status = populate_db(schema, db, &seed);
	printf("Test %s - %s (%d)\n", "populate_db", status==0?"OK":"FAILED", status);

	status = test_search_pk_ck1_ck2(schema, db, &seed);
	printf("Test %s - %s (%d)\n", "test_search_pk_ck1_ck2", status==0?"OK":"FAILED", status);

	status = test_search_pk_ck1(schema, db, &seed);
	printf("Test %s - %s (%d)\n", "test_search_pk_ck1", status==0?"OK":"FAILED", status);

	status = test_search_pk(schema, db, &seed);
	printf("Test %s - %s (%d)\n", "test_search_pk", status==0?"OK":"FAILED", status);

	status = delete_test(schema, db, &seed);
	printf("Test %s - %s (%d)\n", "delete_test", status==0?"OK":"FAILED", status);

	status = test_create_queue(db);
	printf("Test %s - %s (%d)\n", "create_queue", status==0?"OK":"FAILED", status);

	status = test_subscribe_queue(db);
	printf("Test %s - %s (%d)\n", "subscribe_queue", status==0?"OK":"FAILED", status);

	status = test_enqueue(db);
	printf("Test %s - %s (%d)\n", "enqueue", status==0?"OK":"FAILED", status);

	status = test_read_queue(db);
	printf("Test %s - %s (%d)\n", "read_queue", status==0?"OK":"FAILED", status);

	status = test_consume_queue(db);
	printf("Test %s - %s (%d)\n", "consume_queue", status==0?"OK":"FAILED", status);

	status = test_unsubscribe_queue(db);
	printf("Test %s - %s (%d)\n", "unsubscribe_queue", status==0?"OK":"FAILED", status);

	status = test_delete_queue(db);
	printf("Test %s - %s (%d)\n", "delete_queue", status==0?"OK":"FAILED", status);

	status = close_remote_db(db);
	printf("Test %s - %s (%d)\n", "close_remote_db", status==0?"OK":"FAILED", status);

    return 0;
}


