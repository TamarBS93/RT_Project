// #include "dispatch.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>

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
void ambulance_dep(void *args);
void police_dep(void *args);
void fire_dep(void *args);
// Resource Management:
void ambulance(void *args);
void police(void *args);
void fire(void *args);

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

// // resources tasks:
// TaskHandle_t ambTaskHandle;
// TaskHandle_t polTaskHandle;
// TaskHandle_t fireTaskHandle;

// resources semaphores:
SemaphoreHandle_t ambulances;
SemaphoreHandle_t police_cars;
SemaphoreHandle_t fire_trucks;

#define TASK_DURATION 10000

void main_myproject(void)
{
    printf("Hello, from main_myproject \n");

    // Queues creation:
    EventQueue = xQueueCreate(50, sizeof(event_t*));
    LogQueue = xQueueCreate(50, sizeof(event_t*));
    AmbulanceQueue = xQueueCreate(20, sizeof(event_t*));
    PoliceQueue = xQueueCreate(20, sizeof(event_t*));
    FDQueue = xQueueCreate(20, sizeof(event_t*));

    ambulances = xSemaphoreCreateCounting(4,4);
    police_cars = xSemaphoreCreateCounting(3,3);
    fire_trucks = xSemaphoreCreateCounting(2,2);

    // Specify stack size:
    UBaseType_t stackSize = configMINIMAL_STACK_SIZE;

        
    if (xTaskCreate(event_gen, "Event Generator", stackSize,NULL,2,&EventGenTaskHandle) != pdPASS){
        printf("Error creating 'Event Generator' task");
    }
    if (xTaskCreate(dispatch_q, "Dispath Queue", stackSize,NULL,3,&DispatchQTaskHandle) != pdPASS){
        printf("Error creating 'Dispath queue' task");
    }
    if (xTaskCreate(logging, "Logging", stackSize,NULL,1,&LoggingTaskHandle) != pdPASS){
        printf("Error creating 'Logging' task");
    }

    if (xTaskCreate(ambulance_dep, "Ambulance Department", stackSize,NULL,6,&ambulanceTaskHandle) != pdPASS){
        printf("Error creating 'Ambulance Department' task");
    }
    if (xTaskCreate(police_dep, "Police Department", stackSize,NULL,4,&policeTaskHandle) != pdPASS){
        printf("Error creating 'Police Department' task");
    }
    if (xTaskCreate(fire_dep, "Fire Department", stackSize,NULL,5,&fdTaskHandle) != pdPASS){
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
            printf("Error in Writing an event to events queue\n");
        }
        xTaskNotifyGive(DispatchQTaskHandle);        // wake up police_dep
        vTaskDelay(pdMS_TO_TICKS((rand() % 4000 + 1000))); // random time to delay event generation  
    }
}
// Dispatcher Queue:
void dispatch_q(void *args){
  
    printf("Task started: %s\n", pcTaskGetName(NULL));
    event_t *event;

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(EventQueue, &event, 1) != pdPASS)
        {
            printf("Error in Reading an event from event queue\n");
        }
        switch(event->code) {
            case 1: // Police
                xQueueSendToBack(PoliceQueue, &event, 0);  // send pointer
                xTaskNotifyGive(policeTaskHandle);        // wake up police_dep
                break;
            case 2: // Ambulance
                xQueueSendToBack(AmbulanceQueue, &event, 0);  // send pointer
                xTaskNotifyGive(ambulanceTaskHandle);        // wake up ambulance_dep
                break;
            case 3: // fire Department     
                xQueueSendToBack(FDQueue, &event, 0);  // send pointer
                xTaskNotifyGive(fdTaskHandle);        // wake up fire_dep
                break;
        }
        //vTaskDelay(1000); // delay in ticks 
    }

}

// Logging:
void logging(void *args){

    printf("Task started: %s\n", pcTaskGetName(NULL));

    FILE *dispatch_log = fopen("dispatch_log.txt", "w");

    event_t *event;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(LogQueue, &event, 1) != pdPASS)
        {
            printf("Error in Reading an event from Log queue:\n%s\n",strerror(errno) );
        }
        fprintf(dispatch_log, "Event %d: Received call with Code %d\n%s\n", event->id,event->code, event->msg);
        fflush(dispatch_log);
        if  (strstr(event->msg,"The event will remain in the queue for further handling") == NULL){ // NULL- string not found in msg
            // if the evebt is still in queue it should not be freed
            free(event);
        }
        
    }
    fclose(dispatch_log);
}

void ambulance(void *args){
    event_t *event = (event_t *)args;
    TickType_t startTick, endTick, tickSum;
    
    startTick = xTaskGetTickCount();
    vTaskDelay((rand() % TASK_DURATION + 1)); // random ticks to accomplish the event (between 1 to 5)
    xSemaphoreGive(ambulances);
    endTick = xTaskGetTickCount();
    tickSum = endTick - startTick;
    snprintf(event->msg, sizeof(event->msg),
        "A free resource (Ambulance) was allocated for the task, which lasted %lu ticks. The task was completed.\n", tickSum); 
    // logging:
    xTaskNotifyGive(LoggingTaskHandle); // wake up logging
    if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS) printf("Error in Logging an event to log queue\n");  

    vTaskDelete(NULL); // terminate the task

}

void police(void *args){
    event_t *event = (event_t *)args;
    TickType_t startTick, endTick, tickSum;
    
    startTick = xTaskGetTickCount();
    vTaskDelay((rand() % TASK_DURATION + 1)); // random ticks to accomplish the event (between 1 to 5)
    xSemaphoreGive(police_cars);
    endTick = xTaskGetTickCount();
    tickSum = endTick - startTick;
    snprintf(event->msg, sizeof(event->msg),
        "A free resource (Police car) was allocated for the task, which lasted %lu ticks. The task was completed.\n", tickSum); 
    // logging:
    xTaskNotifyGive(LoggingTaskHandle); // wake up logging
    if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS) printf("Error in Logging an event to log queue\n");  
    
    vTaskDelete(NULL); // terminate the task
}

void fire(void *args){
    event_t *event = (event_t *)args;
    TickType_t startTick, endTick, tickSum;
    
    startTick = xTaskGetTickCount();
    vTaskDelay((rand() % TASK_DURATION + 1)); // random ticks to accomplish the event (between 1 to 5)
    xSemaphoreGive(fire_trucks);
    endTick = xTaskGetTickCount();
    tickSum = endTick - startTick;
    snprintf(event->msg, sizeof(event->msg),
        "A free resource (Fire truck) was allocated for the task, which lasted %lu ticks. The task was completed.\n", tickSum); 
    // logging:
    xTaskNotifyGive(LoggingTaskHandle); // wake up logging
    if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS) printf("Error in Logging an event to log queue\n");  
    
    vTaskDelete(NULL); // terminate the task
}

// Emergency Departments:
void ambulance_dep(void *args){
    event_t *event; // = *(event_t *)args;
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(AmbulanceQueue, &event, 0) != pdPASS){
            printf("No event received for ambulance_dep\n");
            continue;
        }
        else {
            printf("Dispatch called an Ambulance.\n");    
            // Resource Management:
            if(xSemaphoreTake(ambulances, 2) == pdFAIL){ //no free ambulance
                // insert back to queue:
                if (xQueueSendToFront(AmbulanceQueue, &event, 1) != pdPASS) printf("Error in Writing an event to ambulance queue\n");                
                // logging:
                snprintf(event->msg, sizeof(event->msg),
                    "The task for the ambulace department failed due to a lack of resources. The event will remain in the queue for further handling.\n"); 
                xTaskNotifyGive(LoggingTaskHandle); // wake up logging
                if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS) printf("Error in Logging an event to log queue\n");  
                continue;
            }
            if (xTaskCreate(ambulance, "Ambulance Unit", configMINIMAL_STACK_SIZE, event, 6, NULL) != pdPASS){
                printf("Failed to create ambulance unit task\n");
                xSemaphoreGive(ambulances); // give back resource if task failed
            }
        }
    }
}

void police_dep(void *args){
    event_t *event; // = *(event_t *)args;
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(PoliceQueue, &event, 0) != pdPASS){
            printf("No event received for the police\n");
            continue;
        }
        else {
            printf("Dispatch called the Police.\n");    
            // Resource Management:
            if(xSemaphoreTake(police_cars, 2) == pdFAIL){ //no free ambulance
                // insert back to queue:
                if (xQueueSendToFront(PoliceQueue, &event, 1) != pdPASS) printf("Error in Writing an event to police queue\n");                
                // logging:
                snprintf(event->msg, sizeof(event->msg),
                    "The task for the police department failed due to a lack of resources. The event will remain in the queue for further handling.\n"); 
                xTaskNotifyGive(LoggingTaskHandle); // wake up logging
                if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS) printf("Error in Logging an event to log queue\n");  
                continue;
            }
            if (xTaskCreate(police, "Police Unit", configMINIMAL_STACK_SIZE, event, 4, NULL) != pdPASS){
                printf("Failed to create police unit task\n");
                xSemaphoreGive(police_cars); // give back resource if task failed
            }        
        }
    }
}

void fire_dep(void *args){
    event_t *event; // = *(event_t *)args;
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(FDQueue, &event, 0) != pdPASS){
            printf("No event received for the fire department\n");
            continue;
        }
        else {
            printf("Dispatch called the Fire Department.\n");    
            // Resource Management:
            if(xSemaphoreTake(fire_trucks, 2) == pdFAIL){ //no free ambulance
                // insert back to queue:
                if (xQueueSendToFront(FDQueue, &event, 1) != pdPASS) printf("Error in Writing an event to police queue\n");                
                // logging:
                snprintf(event->msg, sizeof(event->msg),
                    "The task for the fire department failed due to a lack of resources. The event will remain in the queue for further handling.\n"); 
                xTaskNotifyGive(LoggingTaskHandle); // wake up logging
                if (xQueueSendToBack(LogQueue, &event, 1) != pdPASS) printf("Error in Logging an event to log queue\n");  
                continue;
            }
            if (xTaskCreate(fire, "Fire Unit", configMINIMAL_STACK_SIZE, event, 5, NULL) != pdPASS){
                printf("Failed to create fire unit task\n");
                xSemaphoreGive(fire_trucks); // give back resource if task failed
            }        
        }
    }
}