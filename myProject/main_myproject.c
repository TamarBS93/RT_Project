// #include "dispatch.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

typedef struct{
    int id;
    int code;
    char msg[150];
} event_t;

// Event Generator:
void event_gen(void *args);
// Dispatcher Queue:
void dispatch_q(void *args);
// Emergency Departments:
void Emrgnc_dep(void *args);
// Resource Management:
void ambulance(void *args);
void police(void *args);
void fd(void *args);
// Logging:
void logging(void *args);

// Dispatcher queue;
QueueHandle_t EventQueue;
// Logger queue:
QueueHandle_t LogQueue;
//departments queue:
QueueHandle_t AmbulanceQueue;
QueueHandle_t PoliceQueue;
QueueHandle_t FDQueue;

TaskHandle_t EventGenTaskHandle;
TaskHandle_t DispatchQTaskHandle;
TaskHandle_t LoggingTaskHandle;
// departments tasks:
TaskHandle_t ambulanceTaskHandle;
TaskHandle_t policeTaskHandle;
TaskHandle_t fdTaskHandle;
// resources semaphores:
SemaphoreHandle_t ambulances;
SemaphoreHandle_t police_cars;
SemaphoreHandle_t fire_trucks;

void main_myproject(void)
{
    printf("Hello, from main_myproject \n");

    // Queues creation:
    EventQueue = xQueueCreate(100, sizeof(event_t*));
    LogQueue = xQueueCreate(100, sizeof(event_t*));
    AmbulanceQueue = xQueueCreate(50, sizeof(event_t*));
    PoliceQueue = xQueueCreate(50, sizeof(event_t*));
    FDQueue = xQueueCreate(50, sizeof(event_t*));


    ambulances = xSemaphoreCreateCounting(4,4);
    police_cars = xSemaphoreCreateCounting(3,3);
    fire_trucks = xSemaphoreCreateCounting(2,2);

    // Specify stack size in words
    UBaseType_t stackSize = configMINIMAL_STACK_SIZE*4;
        
    if (xTaskCreate(event_gen, "Event Generator", stackSize,NULL,2,&EventGenTaskHandle) != pdPASS){
        printf("Error creating 'Event Generator' task");
    }
    if (xTaskCreate(dispatch_q, "Dispath Queue", stackSize,NULL,3,&DispatchQTaskHandle) != pdPASS){
        printf("Error creating 'Dispath queue' task");
    }
    if (xTaskCreate(logging, "Logging", stackSize,NULL,1,&LoggingTaskHandle) != pdPASS){
        printf("Error creating 'Logging' task");
    }

    if (xTaskCreate(ambulance, "Ambulance", stackSize,NULL,2,&ambulanceTaskHandle) != pdPASS){
        printf("Error creating 'Ambulance' task");
    }
    if (xTaskCreate(police, "Police", stackSize,NULL,3,&policeTaskHandle) != pdPASS){
        printf("Error creating 'Police' task");
    }
    if (xTaskCreate(fd, "Fire Department", stackSize,NULL,1,&fdTaskHandle) != pdPASS){
        printf("Error creating 'Fire Department' task");
    }
    
    vTaskStartScheduler();
    
    while (1);
}

// Event Generation:
void event_gen(void *args){
    
    printf("Task started: %s\n", pcTaskGetName(NULL));

    int event_id = 0;
    srand((unsigned int)time(NULL)); // Seed random number generator
        
    while(1){
        event_t *event = malloc(sizeof(event_t));
        if(event == NULL) printf("Memory allocation failed!\n");

        event->code = (rand() % 3) + 1;
        event->id = ++event_id;
        // Send the event to the dispatcher queue
        if (xQueueSendToBack(EventQueue, &event, 1) != pdPASS)
        {
            printf("Error in Writing an event to queue\n");
        }
        vTaskDelay(pdMS_TO_TICKS((rand() % 4000 + 1000))); // random time to delay event generation  
    }
}
// Dispatcher Queue:
void dispatch_q(void *args){
  
    printf("Task started: %s\n", pcTaskGetName(NULL));
    event_t *event;

    while(1){
        if (xQueueReceive(EventQueue, &event, 1) != pdPASS)
        {
            printf("Error in Reading an event from queue\n");
        }
        switch(event->code) {
            case 1: // Police
                xQueueSendToBack(PoliceQueue, &event, 0);  // send pointer
                xTaskNotifyGive(policeTaskHandle);        // wake up ambulance
                break;
            case 2: // Ambulance
                xQueueSendToBack(AmbulanceQueue, &event, 0);  // send pointer
                xTaskNotifyGive(ambulanceTaskHandle);        // wake up police
                break;
            case 3: // fire Department     
                xQueueSendToBack(FDQueue, &event, 0);  // send pointer
                xTaskNotifyGive(fdTaskHandle);        // wake up FD
                break;
        }
        vTaskDelay(2); // delay in ticks 
    }

}

// Logging:
void logging(void *args){

    printf("Task started: %s\n", pcTaskGetName(NULL));

    FILE *dispatch_log = fopen("dispatch_log.txt", "w");

    event_t *event;

    while (1)
    {
        if (xQueueReceive(LogQueue, &event, 1) != pdPASS)
        {
            printf("Error in Reading an event from Log queue\n");
        }
        fprintf(dispatch_log, "Event %d: %s\n", event->id, event->msg);
        fflush(dispatch_log);
        free(event);
    }
    fclose(dispatch_log);
}

// Emergency Departments:
void ambulance(void *args){
    event_t *event; // = *(event_t *)args;
    TickType_t startTick, endTick, tickSum;
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(AmbulanceQueue, &event, 0) != pdPASS){
            printf("No event received for ambulance\n");
            continue;
        }
        else {
            printf("Dispatch called an Ambulance:\n");
            startTick = xTaskGetTickCount();
    
            // Resource Management:
            if(xSemaphoreTake(ambulances, 2) == pdFAIL){ // 2 ticks to wait
                // try cars from other departments
                // insert back to queue:
                if (xQueueSendToFront(AmbulanceQueue, &event, 1) != pdPASS)
                {
                    printf("Error in Writing an event to ambulance queue\n");
                }
            }
            else{
                vTaskDelay((rand() % 5 + 1)); // random ticks to accomplish the event (between 1 to 5)
                endTick = xTaskGetTickCount();
                tickSum = endTick - startTick;
            
                xSemaphoreGive(ambulances);
            
                snprintf(event->msg, sizeof(event->msg),
                    "Received call with Code 2 (Ambulance). A free resource was allocated for the task, which lasted %lu ticks. The task was completed.\n", tickSum); 
                // logging(event);
                if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS)
                {
                    printf("Error in Logging an event to log queue\n");
                }
            }
        }
    }

}

void police(void *args){
    event_t *event; // = *(event_t *)args;
    TickType_t startTick, endTick, tickSum;
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(PoliceQueue, &event, 0) != pdPASS){
            printf("No event received for Police\n");
            continue;
        }
        else {
            printf("Dispatch called the Police:\n");
            startTick = xTaskGetTickCount();
    
            // Resource Management:
            if(xSemaphoreTake(police_cars, 2) == pdFAIL){ // 2 ticks to wait
                // try cars from other departments
                // insert back to queue:
                if (xQueueSendToFront(PoliceQueue, &event, 1) != pdPASS)
                {
                    printf("Error in Writing an event to police queue\n");
                }
            }
            else{
                vTaskDelay((rand() % 5 + 1)); // random ticks to accomplish the event (between 1 to 5)
                endTick = xTaskGetTickCount();
                tickSum = endTick - startTick;
            
                xSemaphoreGive(police_cars);
            
                snprintf(event->msg, sizeof(event->msg),
                    "Received call with Code 1 (Police). A free resource was allocated for the task, which lasted %lu ticks. The task was completed.\n", tickSum); 
                // logging(event);
                if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS)
                {
                    printf("Error in Logging an event to log queue\n");
                }
            }
        }
    }
}

void fd(void *args){
    event_t *event; // = *(event_t *)args;
    TickType_t startTick, endTick, tickSum;
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(FDQueue, &event, 0) != pdPASS){
            printf("No event received for Fire Department\n");
            continue;
        }
        else {
            printf("Dispatch called the Fire Department:\n");
            startTick = xTaskGetTickCount();
    
            // Resource Management:
            if(xSemaphoreTake(fire_trucks, 2) == pdFAIL){ // 2 ticks to wait
                // try cars from other departments
                // insert back to queue:
                if (xQueueSendToFront(FDQueue, &event, 1) != pdPASS)
                {
                    printf("Error in Writing an event to FD queue\n");
                }
            }
            else{
                vTaskDelay((rand() % 5 + 1)); // random ticks to accomplish the event (between 1 to 5)
                endTick = xTaskGetTickCount();
                tickSum = endTick - startTick;
            
                xSemaphoreGive(fire_trucks);
            
                snprintf(event->msg, sizeof(event->msg),
                    "Received call with Code 3 (Fire Department). A free resource was allocated for the task, which lasted %lu ticks. The task was completed.\n", tickSum); 
                // logging(event);
                if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS)
                {
                    printf("Error in Logging an event to log queue\n");
                }
            }
        }
    }
}
