#include "mig_st_queue.h"
#include "mig_error_codes.h"

/************************************************/
/* EXPORTED FUNCTIONS */
/************************************************/
int
mig_queue_init ( mig_queue_t *queue ,
		 int max_queue_len )
{
	if ( ( queue == NULL ) ||
	     ( max_queue_len <= 0 ) )
		return MIG_ERROR_PARAM;

	queue->head = NULL;
	queue->tail = NULL;
	queue->len = 0;
	queue->max_len = max_queue_len;

	sem_init ( &( queue->empty ) , 0  , max_queue_len );
	sem_init ( &( queue->full ) , 0 , 0 );

	pthread_mutex_init ( &( queue->mutex ) , NULL );
	pthread_cond_init ( &( queue->cond ) , NULL );

	return MIG_OK;
}

/************************************************/
int
mig_queue_add ( mig_queue_t *queue , void *data )
{
	int rc;
	mig_entry_t *tmp;

	if ( queue == NULL )
		return MIG_ERROR_PARAM;

	tmp = (mig_entry_t*)
		malloc ( sizeof( mig_entry_t ) );

	if ( tmp == NULL )
		return MIG_ERROR_MEMORY;

	tmp->data = data;
    	tmp->next = NULL;

	/* wait for free space in the queue */
	sem_wait ( &( queue->empty ) );

    /* assure exclusive access to the list */
	rc = pthread_mutex_lock ( &( queue->mutex )  );

    /* add new request to the end of the list */
	if ( queue->len == 0 )
	/* special case - list is empty */
	{
		queue->head = tmp;
		queue->tail = tmp;
	}
	else
	{
		queue->tail->next = tmp;
		queue->tail = tmp;
	}

	/* increase total number by one */
	queue->len ++;

	/* unlock mutex */
	rc = pthread_mutex_unlock ( &( queue->mutex ) );

	/* signal there's a new request to handle */
	//rc = pthread_cond_signal( queue->p_cond_var );
	sem_post ( &( queue->full ) );

	return MIG_OK;
}

/************************************************/
void*
mig_queue_get ( mig_queue_t *queue )
{
	int rc;
	mig_entry_t *tmp;
	void *tmp_data;

   if ( queue == NULL )
      return NULL;
   
   /* wait for new items in the queue */
   sem_wait ( &( queue->full ) );
   
   /* assure exclusive access to the list */
	rc = pthread_mutex_lock ( &( queue->mutex ) );
   
	if ( queue->len > 0 )
	{
		tmp = queue->head;
		queue->head = tmp->next;
      
		/* this was last entry ? */
		if ( queue->head == NULL )
			queue->tail = NULL;
      
		queue->len --;
	}
	else /* queue is empty */
		tmp = NULL;
   
	/* unlock mutex */
	rc = pthread_mutex_unlock ( &( queue->mutex ) );
   
	/* wait for new items in the queue */
	sem_post ( &( queue->empty ) );
   
	if ( tmp == NULL )
		return NULL;
   
	tmp_data = tmp->data;
	free ( tmp );
	return tmp_data;
}

/************************************************/
int
mig_queue_get_len ( mig_queue_t *queue )
{
	int rc;
	int len;

	if ( queue == NULL )
		return -1;

	rc = pthread_mutex_lock ( &( queue->mutex ) );
	len = queue->len;
	rc = pthread_mutex_unlock ( &( queue->mutex ) );

	return len;
}

/************************************************/
int
mig_queue_get_max_len ( mig_queue_t *queue )
{
	int rc;
	int max_len;

	if ( queue == NULL )
		return -1;

	rc = pthread_mutex_lock ( &( queue->mutex ) );
	max_len = queue->max_len;
	rc = pthread_mutex_unlock ( &( queue->mutex ) );

	return max_len;
}

/************************************************/
void
mig_queue_del ( mig_queue_t *queue , queue_free_f f )
{
	void *data;

   if ( queue == NULL )
      return;
   
	while ( queue->len > 0 )
	{
		data = mig_queue_get ( queue );
		if ( data )
			f ( data );
	}
   
	pthread_mutex_destroy ( &( queue->mutex ) );
	pthread_cond_destroy ( &( queue->cond ) );
   
	sem_destroy ( &( queue->empty ) );
	sem_destroy ( &( queue->full ) );
   
	free ( queue );
}

/************************************************/
void
mig_queue_dump ( mig_queue_t *queue, queue_dump_f f )
{
	int i , rc;
	mig_entry_t *tmp;
   
	if ( queue == NULL ||
	     f == NULL )
		return;

	rc = pthread_mutex_lock ( &( queue->mutex ) );
	tmp = queue->head;
	for ( i = 0 ; i < queue->len ; ++i , tmp = tmp->next )
		f( tmp->data );
	rc = pthread_mutex_unlock ( &( queue->mutex ) );
}

