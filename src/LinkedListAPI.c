/*
Name: David Entonu
Student Number:1286123
*/

#include <stdlib.h>
#include <string.h>
#include "LinkedListAPI.h"

List* initializeList(char* (*printFunction)(void* toBePrinted),
                       void (*deleteFunction)(void* toBeDeleted),
                       int (*compareFunction)(const void* first, const void* second)) {
    if (printFunction == NULL || deleteFunction == NULL || compareFunction == NULL)
        return NULL;

    List* newList = (List*) malloc(sizeof(List));
    if (newList == NULL)
        return NULL;

    newList->head = NULL;
    newList->tail = NULL;
    newList->length = 0;
    newList->printData = printFunction;
    newList->deleteData = deleteFunction;
    newList->compare = compareFunction;

    return newList;
}

Node* initializeNode(void* data) {
    Node* newNode = (Node*) malloc(sizeof(Node));
    if (newNode == NULL)
        return NULL;
    newNode->data = data;
    newNode->previous = NULL;
    newNode->next = NULL;
    return newNode;
}

void insertFront(List* list, void* toBeAdded) {
    if (list == NULL)
        return;
    Node* newNode = initializeNode(toBeAdded);
    if (newNode == NULL)
        return;
    newNode->next = list->head;
    newNode->previous = NULL;
    if (list->head != NULL)
        list->head->previous = newNode;
    else
        list->tail = newNode;
    list->head = newNode;
    list->length++;
}

void insertBack(List* list, void* toBeAdded) {
    if (list == NULL)
        return;
    Node* newNode = initializeNode(toBeAdded);
    if (newNode == NULL)
        return;
    newNode->previous = list->tail;
    newNode->next = NULL;
    if (list->tail != NULL)
        list->tail->next = newNode;
    else
        list->head = newNode;
    list->tail = newNode;
    list->length++;
}

void freeList(List* list) {
    if (list == NULL)
        return;
    Node* current = list->head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        if (list->deleteData && temp->data)
            list->deleteData(temp->data);
        free(temp);
    }
    free(list);
}

void clearList(List* list) {
    if (list == NULL)
        return;
    Node* current = list->head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        if (list->deleteData && temp->data)
            list->deleteData(temp->data);
        free(temp);
    }
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
}

void insertSorted(List* list, void* toBeAdded) {
    if (list == NULL)
        return;
    Node* newNode = initializeNode(toBeAdded);
    if (newNode == NULL)
        return;
    if (list->head == NULL) {
        list->head = newNode;
        list->tail = newNode;
        list->length++;
        return;
    }
    Node* current = list->head;
    while (current != NULL) {
        if (list->compare(toBeAdded, current->data) <= 0)
            break;
        current = current->next;
    }
    if (current == list->head) {
        newNode->next = list->head;
        newNode->previous = NULL;
        list->head->previous = newNode;
        list->head = newNode;
    } else if (current == NULL) {
        newNode->previous = list->tail;
        newNode->next = NULL;
        list->tail->next = newNode;
        list->tail = newNode;
    } else {
        newNode->next = current;
        newNode->previous = current->previous;
        current->previous->next = newNode;
        current->previous = newNode;
    }
    list->length++;
}

void* deleteDataFromList(List* list, void* toBeDeleted) {
    if (list == NULL || list->head == NULL)
        return NULL;
    Node* current = list->head;
    while (current != NULL) {
        if (list->compare(current->data, toBeDeleted) == 0) {
            void* foundData = current->data;
            if (current->previous != NULL)
                current->previous->next = current->next;
            else
                list->head = current->next;
            if (current->next != NULL)
                current->next->previous = current->previous;
            else
                list->tail = current->previous;
            free(current);
            list->length--;
            return foundData;
        }
        current = current->next;
    }
    return NULL;
}

void* getFromFront(List* list) {
    if (list == NULL || list->head == NULL)
        return NULL;
    return list->head->data;
}

void* getFromBack(List* list) {
    if (list == NULL || list->tail == NULL)
        return NULL;
    return list->tail->data;
}

char* toString(List* list) {
    if (list == NULL)
        return NULL;
    int bufferSize = 1024;
    char* result = (char*) malloc(bufferSize);
    if (result == NULL)
        return NULL;
    result[0] = '\0';
    Node* current = list->head;
    while (current != NULL) {
        char* nodeStr = list->printData(current->data);
        if (nodeStr == NULL)
            nodeStr = strdup("NULL");
        int neededSize = strlen(result) + strlen(nodeStr) + 2;
        if (neededSize > bufferSize) {
            bufferSize *= 2;
            char* temp = realloc(result, bufferSize);
            if (temp == NULL) {
                free(result);
                free(nodeStr);
                return NULL;
            }
            result = temp;
        }
        strcat(result, nodeStr);
        strcat(result, "\n");
        free(nodeStr);
        current = current->next;
    }
    return result;
}

ListIterator createIterator(List* list) {
    ListIterator iter;
    if (list == NULL)
        iter.current = NULL;
    else
        iter.current = list->head;
    return iter;
}

void* nextElement(ListIterator* iter) {
    if (iter == NULL || iter->current == NULL)
        return NULL;
    void* data = iter->current->data;
    iter->current = iter->current->next;
    return data;
}

int getLength(List* list) {
    if (list == NULL)
        return -1;
    return list->length;
}

void* findElement(List* list, bool (*customCompare)(const void* first, const void* second), const void* searchRecord) {
    if (list == NULL || customCompare == NULL)
        return NULL;
    Node* current = list->head;
    while (current != NULL) {
        if (customCompare(current->data, searchRecord))
            return current->data;
        current = current->next;
    }
    return NULL;
}
