#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include "eventbuf.h"

int numProducers;
int numConsumers;
int producerEvents;
int maxEvents;

struct eventbuf *event_buffer;

sem_t *available_events_sem;
sem_t *event_sem;
sem_t *producer_consumer_mutex_sem;


sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    sem = sem_open(name, O_CREAT, 0600, value);
    if (sem == SEM_FAILED)
        return SEM_FAILED;

    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}
void *consumer_run(void *consumer_id) {
    int *id = consumer_id;

    while (1) {
        sem_wait(available_events_sem);
        sem_wait(producer_consumer_mutex_sem);

        if (eventbuf_empty(event_buffer) == 1) {
            sem_post(producer_consumer_mutex_sem);
            break;
        }
        
        int event_number = eventbuf_get(event_buffer);

        printf("C%d: got event %d\n", *id, event_number);

        sem_post(producer_consumer_mutex_sem);
        sem_post(event_sem);
    }

    printf("C%d: exiting\n", *id);

    return NULL;

}

void *producer_run(void *producer_id) {

    int* id = producer_id;

    for (int i = 0; i < producerEvents; i++) {
        int event_number = (*id * 100) + i;
        sem_wait(event_sem);
        sem_wait(producer_consumer_mutex_sem);

        printf("P%d: adding event %d\n", *id, event_number);
        
        eventbuf_add(event_buffer, event_number);
        
        sem_post(producer_consumer_mutex_sem);
        sem_post(available_events_sem);
    }

    printf("P%d: exiting\n", *id);

    return NULL;

}

int main(int argc, char *argv[]) {
    
    if (argc != 5) {
        fprintf(stderr, "usage: producers consumers producerEvents maxEvents\n");
        exit(1);
    }

    numProducers = atoi(argv[1]);
    numConsumers = atoi(argv[2]);
    producerEvents = atoi(argv[3]);
    maxEvents = atoi(argv[4]);

    event_sem = sem_open_temp("event_sem", maxEvents);
    available_events_sem = sem_open_temp("available_events_sem", 0);
    producer_consumer_mutex_sem = sem_open_temp("producer_consumer_mutex_sem", 1);

    event_buffer = eventbuf_create();

    int *producer_thread_id = calloc(numProducers, sizeof *producer_thread_id);
    int *consumer_thread_id = calloc(numConsumers, sizeof *consumer_thread_id);

    pthread_t *producer_thread = calloc(numProducers, sizeof *producer_thread);
    pthread_t *consumer_thread = calloc(numConsumers, sizeof *consumer_thread);

    for (int i = 0; i < numProducers; i++) {
        producer_thread_id[i] = i;
        pthread_create(producer_thread + i, NULL, producer_run, producer_thread_id + i);
    }
    
    for (int i = 0; i < numConsumers; i++) {
        consumer_thread_id[i] = i;
        pthread_create(consumer_thread + i, NULL, consumer_run, consumer_thread_id + i);
    }

    for (int i = 0; i < numProducers; i++) {
        pthread_join(producer_thread[i], NULL);
    }

    for (int i = 0; i < numConsumers; i++) {
        sem_post(available_events_sem);
    }

    for (int i = 0; i < numConsumers; i++) {
        pthread_join(consumer_thread[i], NULL);
    }

    eventbuf_free(event_buffer);
}