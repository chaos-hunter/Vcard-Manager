/**
 *Name: David Entonu
 *Student Number:1286123
 */

#include "VCParser.h"
#include "VCHelpers.h"
#include <stdlib.h>
#include <ctype.h>


/**
 * Extra helper functions
 */
 char** splitValues(const char* input, int* count) {

    char** values = malloc(5 * sizeof(char*));
    if (!values) return NULL;
    *count = 0;

    char* temp = strdup(input);
    if (!temp) {
        free(values);
        return NULL;
    }
    char* token = temp;
    char* next;

    for (int i = 0; i < 5; i++) {
        next = strchr(token, ';');
        if (next != NULL) {
            *next = '\0';
            values[i] = strdup(token);
            token = next + 1;
        } else {
            values[i] = strdup(token);
            break;
        }
    }
    *count = 5;
    free(temp);
    return values;
}

DateTime *parsedateProperty(const char *dateStr, bool isTextValue) {
    if (dateStr == NULL)
        return NULL;
    
    char *temp = strdup(dateStr);
    if (temp == NULL)
        return NULL;
    
    char *start = temp;
    while (*start && isspace((unsigned char)*start))
        start++;
    
    // Remove trailing whitespace.
    size_t len = strlen(start);
    if (len > 0) {
        char *end = start + len - 1;
        while (end > start && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
    }
    
    // Duplicate the trimmed string into a new buffer.
    char *trimmed = strdup(start);
    free(temp);  
    if (trimmed == NULL)
        return NULL;
    
    DateTime *datetime = malloc(sizeof(DateTime));
    if (datetime == NULL) {
        free(trimmed);
        return NULL;
    }
    
    // Allocate fixed-size buffers.
    datetime->date = calloc(11, sizeof(char));  
    datetime->time = calloc(9, sizeof(char));   
    datetime->text = calloc(strlen(trimmed) + 2, sizeof(char));
    
    if (!datetime->date || !datetime->time || !datetime->text) {
        deleteDate(datetime);
        free(trimmed);
        return NULL;
    }
    
    datetime->UTC = false;
    datetime->isText = false;
    
    if (isTextValue) {
        datetime->isText = true;
        strncpy(datetime->text, trimmed, strlen(trimmed) + 1);
        datetime->date[0] = '\0';
        datetime->time[0] = '\0';
    } else if (strchr(trimmed, 'T') != NULL) {
        char *tPos = strchr(trimmed, 'T');
        size_t dateLen = tPos - trimmed;
        strncpy(datetime->date, trimmed, dateLen);
        datetime->date[dateLen] = '\0';
        strncpy(datetime->time, tPos + 1, 6);
        datetime->time[6] = '\0';
    } else if (trimmed[0] == 'T') {
        strncpy(datetime->time, trimmed + 1, 6);
        datetime->time[6] = '\0';
        datetime->date[0] = '\0';
    } else {
        strncpy(datetime->date, trimmed, 8);
        datetime->date[8] = '\0';
        datetime->time[0] = '\0';
    }
    
    size_t timeLen = strlen(datetime->time);
    if (timeLen > 0 && datetime->time[timeLen - 1] == 'Z') {
        datetime->UTC = true;
        datetime->time[timeLen - 1] = '\0';
    }
    
    free(trimmed);
    return datetime;
}

/***MAIN FUNCTIONS */

VCardErrorCode createCard(char *fileName, Card **obj) {
    if (obj == NULL)
        return INV_FILE;
    *obj = NULL;

    if (fileName == NULL || strlen(fileName) == 0)
        return INV_FILE;

    FILE *file = fopen(fileName, "rb");
    if (file == NULL)
        return INV_FILE;

    Card *newCard = (Card *) malloc(sizeof(Card));
    if (newCard == NULL) {
        fclose(file);
        return OTHER_ERROR;
    }
    newCard->fn = NULL;
    newCard->optionalProperties = initializeList(propertyToString, deleteProperty, compareProperties);
    newCard->birthday = NULL;
    newCard->anniversary = NULL;

    bool begin = false;
    bool end = false;
    bool foundVersion = false;
    bool foundFN = false;

    char line[1024] = "";
    char unfoldedLine[4096] = "";
    unfoldedLine[0] = '\0';

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t len = strlen(line);
        if (len < 2 || line[len-2] != '\r' || line[len-1] != '\n') {
            fclose(file);
            deleteCard(newCard);
            return INV_CARD;
        }

        line[len-2] = '\0';

        if (strlen(line) == 0)
            continue;

        if ((line[0] == ' ' || line[0] == '\t') && strlen(unfoldedLine) > 0) {
            char *continuedPart = line;
            while (*continuedPart && isspace((unsigned char)*continuedPart))
                continuedPart++;
            strcat(unfoldedLine, continuedPart);
        } else {
            if (strlen(unfoldedLine) > 0) {
                char *lineCopy = strdup(unfoldedLine);
                if (lineCopy == NULL) {
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }
                char *trimmed = lineCopy;
                while (*trimmed && isspace((unsigned char)*trimmed))
                    trimmed++;
                size_t trimmedLen = strlen(trimmed);
                if (trimmedLen > 0) {
                    char *endPtr = trimmed + trimmedLen - 1;
                    while (endPtr > trimmed && isspace((unsigned char)*endPtr)) {
                        *endPtr = '\0';
                        endPtr--;
                    }
                }
                if (strcmp(trimmed, "BEGIN:VCARD") == 0)
                    begin = true;
                else if (strcmp(trimmed, "END:VCARD") == 0) {
                    end = true;
                    free(lineCopy);
                    unfoldedLine[0] = '\0';
                    break;
                } else {
                    char *colonPtr = strchr(trimmed, ':');
                    if (colonPtr == NULL) {
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }
                    *colonPtr = '\0';
                    char *descriptor = trimmed;
                    char *valueSection = colonPtr + 1;
                    if (strlen(valueSection) == 0) {
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }
                    char *token = strtok(descriptor, ";");
                    if (token == NULL) {
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }
                    char *groupName = "";
                    char *propertyName = token;
                    char *dotPtr = strchr(token, '.');
                    if (dotPtr != NULL) {
                        *dotPtr = '\0';
                        groupName = token;
                        propertyName = dotPtr + 1;
                    }
                    if (strlen(propertyName) == 0) {
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }

                    Property *prop = (Property *) malloc(sizeof(Property));
                    if (prop == NULL) {
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return OTHER_ERROR;
                    }
                    prop->name = strdup(propertyName);
                    prop->group = strdup(groupName);
                    if (prop->name == NULL || prop->group == NULL) {
                        free(prop->name);
                        free(prop->group);
                        free(prop);
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return OTHER_ERROR;
                    }
                    prop->parameters = initializeList(parameterToString, deleteParameter, compareParameters);
                    prop->values = initializeList(valueToString, deleteValue, compareValues);
                    if (prop->parameters == NULL || prop->values == NULL) {
                        if (prop->parameters) freeList(prop->parameters);
                        if (prop->values) freeList(prop->values);
                        free(prop->name);
                        free(prop->group);
                        free(prop);
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return OTHER_ERROR;
                    }

                    while ((token = strtok(NULL, ";")) != NULL) {
                        char *equalPtr = strchr(token, '=');
                        if (equalPtr == NULL) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return INV_PROP;
                        }
                        *equalPtr = '\0';
                        char *paramName = token;
                        char *paramValue = equalPtr + 1;
                        if (strlen(paramName) == 0 || strlen(paramValue) == 0) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return INV_PROP;
                        }
                        Parameter *param = (Parameter *) malloc(sizeof(Parameter));
                        if (param == NULL) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return OTHER_ERROR;
                        }
                        param->name = strdup(paramName);
                        param->value = strdup(paramValue);
                        if (param->name == NULL || param->value == NULL) {
                            if (param->name) free(param->name);
                            if (param->value) free(param->value);
                            free(param);
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return OTHER_ERROR;
                        }
                        insertBack(prop->parameters, param);
                    }

                    if (strcmp(propertyName, "N") == 0) {
                        int compCount = 0;
                        char **components = splitValues(valueSection, &compCount);
                        if (components == NULL) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return OTHER_ERROR;
                        }
                        for (int i = 0; i < compCount; i++) {
                            char *cleanComponent = components[i];
                            while (*cleanComponent && isspace((unsigned char)*cleanComponent))
                                cleanComponent++;
                            size_t lenComp = strlen(cleanComponent);
                            if (lenComp > 0) {
                                char *end = cleanComponent + lenComp - 1;
                                while (end > cleanComponent && isspace((unsigned char)*end)) {
                                    *end = '\0';
                                    end--;
                                }
                            }
                            char *valueCopy = strdup(cleanComponent);
                            if (valueCopy == NULL) {
                                for (int j = 0; j < compCount; j++) {
                                    free(components[j]);
                                }
                                free(components);
                                deleteProperty(prop);
                                free(lineCopy);
                                fclose(file);
                                deleteCard(newCard);
                                return OTHER_ERROR;
                            }
                            insertBack(prop->values, valueCopy);
                        }
                        for (int i = 0; i < compCount; i++) {
                            free(components[i]);
                        }
                        free(components);
                    }
                    else if (strcmp(prop->name, "BDAY") == 0 || strcmp(prop->name, "ANNIVERSARY") == 0) {
                        char *cleanValue = valueSection;
                        while (*cleanValue && isspace((unsigned char)*cleanValue))
                            cleanValue++;
                        size_t lenVal = strlen(cleanValue);
                        if (lenVal > 0) {
                            char *end = cleanValue + lenVal - 1;
                            while (end > cleanValue && isspace((unsigned char)*end)) {
                                *end = '\0';
                                end--;
                            }
                        }
                        bool isTextValue = false;
                        if (prop->parameters != NULL && prop->parameters->head != NULL) {
                            Node *curr = prop->parameters->head;
                            while (curr != NULL) {
                                Parameter *param = (Parameter *) curr->data;
                                if (strcmp(param->name, "VALUE") == 0 &&
                                    strcmp(param->value, "text") == 0) {
                                    isTextValue = true;
                                    break;
                                }
                                curr = curr->next;
                            }
                        }
                        DateTime *datetime = parsedateProperty(cleanValue, isTextValue);
                        if (!datetime) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return INV_DT;
                        }
                        if (strcmp(prop->name, "BDAY") == 0) {
                            if (newCard->birthday) deleteDate(newCard->birthday);
                            newCard->birthday = datetime;
                        } else {
                            if (newCard->anniversary) deleteDate(newCard->anniversary);
                            newCard->anniversary = datetime;
                        }
                        deleteProperty(prop);
                        free(lineCopy);
                        continue;
                    }
                    else {
                        char *valueToken = strtok(valueSection, ",");
                        if (valueToken == NULL) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return INV_PROP;
                        }
                        while (valueToken != NULL) {
                            char *cleanValue = valueToken;
                            while (*cleanValue && isspace((unsigned char)*cleanValue))
                                cleanValue++;
                            size_t lenVal = strlen(cleanValue);
                            if (lenVal > 0) {
                                char *end = cleanValue + lenVal - 1;
                                while (end > cleanValue && isspace((unsigned char)*end)) {
                                    *end = '\0';
                                    end--;
                                }
                            }
                            if (strlen(cleanValue) == 0) {
                                deleteProperty(prop);
                                free(lineCopy);
                                fclose(file);
                                deleteCard(newCard);
                                return INV_PROP;
                            }
                            char *valueCopy = strdup(cleanValue);
                            if (valueCopy == NULL) {
                                deleteProperty(prop);
                                free(lineCopy);
                                fclose(file);
                                deleteCard(newCard);
                                return OTHER_ERROR;
                            }
                            insertBack(prop->values, valueCopy);
                            valueToken = strtok(NULL, ",");
                        }
                    }

                    if (strcmp(prop->name, "VERSION") == 0) {
                        char *verText = (char *) getFromFront(prop->values);
                        if (strcmp(verText, "4.0") != 0) {
                            deleteProperty(prop);
                            free(lineCopy);
                            fclose(file);
                            deleteCard(newCard);
                            return INV_CARD;
                        }
                        foundVersion = true;
                        deleteProperty(prop);
                    }
                    else if (strcmp(prop->name, "FN") == 0) {
                        if (!(foundFN)) {
                            newCard->fn = prop;
                            foundFN = true;
                        } else {
                            insertBack(newCard->optionalProperties, prop);
                        }
                    }
                    else {
                        insertBack(newCard->optionalProperties, prop);
                    }
                }
                free(lineCopy);
                unfoldedLine[0] = '\0';
            }
            strcpy(unfoldedLine, line);
        }
    }

    if (strlen(unfoldedLine) > 0 && !end) {
        char *lineCopy = strdup(unfoldedLine);
        if (lineCopy == NULL) {
            fclose(file);
            deleteCard(newCard);
            return OTHER_ERROR;
        }

        char *trimmed = lineCopy;
        while (*trimmed && isspace((unsigned char)*trimmed))
            trimmed++;
        size_t trimmedLen = strlen(trimmed);
        if (trimmedLen > 0) {
            char *endPtr = trimmed + trimmedLen - 1;
            while (endPtr > trimmed && isspace((unsigned char)*endPtr)) {
                *endPtr = '\0';
                endPtr--;
            }
        }
        if (strcmp(trimmed, "END:VCARD") == 0)
            end = true;
        else {

            char *colonPtr = strchr(trimmed, ':');
            if (colonPtr == NULL) {
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            }
            *colonPtr = '\0';
            char *descriptor = trimmed;
            char *valueSection = colonPtr + 1;
            if (strlen(valueSection) == 0) {
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            }
            char *token = strtok(descriptor, ";");
            if (token == NULL) {
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            }
            char *groupName = "";
            char *propertyName = token;
            char *dotPtr = strchr(token, '.');
            if (dotPtr != NULL) {
                *dotPtr = '\0';
                groupName = token;
                propertyName = dotPtr + 1;
            }
            if (strlen(propertyName) == 0) {
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            }
            Property *prop = malloc(sizeof(Property));
            if (prop == NULL) {
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return OTHER_ERROR;
            }
            prop->name = strdup(propertyName);
            prop->group = strdup(groupName);
            if (prop->name == NULL || prop->group == NULL) {
                free(prop->name);
                free(prop->group);
                free(prop);
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return OTHER_ERROR;
            }
            prop->parameters = initializeList(parameterToString, deleteParameter, compareParameters);
            prop->values = initializeList(valueToString, deleteValue, compareValues);
            if (prop->parameters == NULL || prop->values == NULL) {
                if (prop->parameters) freeList(prop->parameters);
                if (prop->values) freeList(prop->values);
                free(prop->name);
                free(prop->group);
                free(prop);
                free(lineCopy);
                fclose(file);
                deleteCard(newCard);
                return OTHER_ERROR;
            }
            while ((token = strtok(NULL, ";")) != NULL) {
                char *equalPtr = strchr(token, '=');
                if (equalPtr == NULL) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }
                *equalPtr = '\0';
                char *paramName = token;
                char *paramValue = equalPtr + 1;
                if (strlen(paramName) == 0 || strlen(paramValue) == 0) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }
                Parameter *param = malloc(sizeof(Parameter));
                if (param == NULL) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }
                param->name = strdup(paramName);
                param->value = strdup(paramValue);
                if (param->name == NULL || param->value == NULL) {
                    if (param->name) free(param->name);
                    if (param->value) free(param->value);
                    free(param);
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }
                insertBack(prop->parameters, param);
            }
            if (strcmp(propertyName, "N") == 0) {
                int compCount = 0;
                char **components = splitValues(valueSection, &compCount);
                if (components == NULL) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }
                for (int i = 0; i < compCount; i++) {
                    char *cleanComponent = components[i];
                    while (*cleanComponent && isspace((unsigned char)*cleanComponent))
                        cleanComponent++;
                    size_t lenComp = strlen(cleanComponent);
                    if (lenComp > 0) {
                        char *end = cleanComponent + lenComp - 1;
                        while (end > cleanComponent && isspace((unsigned char)*end)) {
                            *end = '\0';
                            end--;
                        }
                    }
                    char *valueCopy = strdup(cleanComponent);
                    if (valueCopy == NULL) {
                        for (int j = 0; j < compCount; j++) {
                            free(components[j]);
                        }
                        free(components);
                        deleteProperty(prop);
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return OTHER_ERROR;
                    }
                    insertBack(prop->values, valueCopy);
                }
                for (int i = 0; i < compCount; i++) {
                    free(components[i]);
                }
                free(components);
            }
            else if (strcmp(prop->name, "BDAY") == 0 || strcmp(prop->name, "ANNIVERSARY") == 0) {
                char *cleanValue = valueSection;
                while (*cleanValue && isspace((unsigned char)*cleanValue))
                    cleanValue++;
                size_t lenVal = strlen(cleanValue);
                if (lenVal > 0) {
                    char *end = cleanValue + lenVal - 1;
                    while (end > cleanValue && isspace((unsigned char)*end)) {
                        *end = '\0';
                        end--;
                    }
                }
                bool isTextValue = false;
                if (prop->parameters != NULL && prop->parameters->head != NULL) {
                    Node *current = prop->parameters->head;
                    while (current != NULL) {
                        Parameter *param = (Parameter *) current->data;
                        if (strcmp(param->name, "VALUE") == 0 &&
                            strcmp(param->value, "text") == 0) {
                            isTextValue = true;
                            break;
                        }
                        current = current->next;
                    }
                }
                DateTime *datetime = parsedateProperty(cleanValue, isTextValue);
                if (!datetime) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_DT;
                }
                if (strcmp(prop->name, "BDAY") == 0) {
                    if (newCard->birthday) deleteDate(newCard->birthday);
                    newCard->birthday = datetime;
                } else {
                    if (newCard->anniversary) deleteDate(newCard->anniversary);
                    newCard->anniversary = datetime;
                }
                deleteProperty(prop);
                free(lineCopy);
                fclose(file);
                *obj = newCard;
                return OK;
            }
            else {
                char *valueToken = strtok(valueSection, ",");
                if (valueToken == NULL) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }
                while (valueToken != NULL) {
                    char *cleanValue = valueToken;
                    while (*cleanValue && isspace((unsigned char)*cleanValue))
                        cleanValue++;
                    size_t lenVal = strlen(cleanValue);
                    if (lenVal > 0) {
                        char *end = cleanValue + lenVal - 1;
                        while (end > cleanValue && isspace((unsigned char)*end)) {
                            *end = '\0';
                            end--;
                        }
                    }
                    if (strlen(cleanValue) == 0) {
                        deleteProperty(prop);
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }
                    char *valueCopy = strdup(cleanValue);
                    if (valueCopy == NULL) {
                        deleteProperty(prop);
                        free(lineCopy);
                        fclose(file);
                        deleteCard(newCard);
                        return OTHER_ERROR;
                    }
                    insertBack(prop->values, valueCopy);
                    valueToken = strtok(NULL, ",");
                }
            }
            if (strcmp(prop->name, "VERSION") == 0) {
                char *verText = (char *) getFromFront(prop->values);
                if (strcmp(verText, "4.0") != 0) {
                    deleteProperty(prop);
                    free(lineCopy);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_CARD;
                }
                foundVersion = true;
                deleteProperty(prop);
            }
            else if (strcmp(prop->name, "FN") == 0) {
                if (!(foundFN)) {
                    newCard->fn = prop;
                    foundFN = true;
                } else {
                    insertBack(newCard->optionalProperties, prop);
                }
            }
            else {
                insertBack(newCard->optionalProperties, prop);
            }
            free(lineCopy);
        }
    }

    fclose(file);

    if (!begin || !end || !foundFN) {
        deleteCard(newCard);
        return INV_CARD;
    }

    *obj = newCard;
    return OK;
}



//Frees all memory allocated for the Card object when called
void deleteCard(Card* obj) {
    if (obj == NULL)
        return;
    if (obj->fn != NULL)
        deleteProperty(obj->fn);
    if (obj->optionalProperties != NULL)
        freeList(obj->optionalProperties);
    if (obj->birthday != NULL)
        deleteDate(obj->birthday);
    if (obj->anniversary != NULL)
        deleteDate(obj->anniversary);
    free(obj);
}

//Returns a string representation of the Card file.
char* cardToString(const Card *cardObj) {
    if (cardObj == NULL) {
        char *nullStr = (char *) malloc(5);
        if (nullStr != NULL)
            strcpy(nullStr, "null");
        return nullStr;
    }

    char *result = (char *) malloc(4096);
    if (result == NULL)
        return NULL;
    result[0] = '\0';

    strcat(result, "vCard:\n");
    if (cardObj->fn != NULL) {
        char *fnStr = propertyToString(cardObj->fn);
        strcat(result, "FN: ");
        strcat(result, fnStr);
        strcat(result, "\n");
        free(fnStr);
    }
    if (cardObj->optionalProperties != NULL && cardObj->optionalProperties->head != NULL) {
        strcat(result, "Additional Properties:\n");
        Node *current = cardObj->optionalProperties->head;
        while (current != NULL) {
            char *propStr = propertyToString((Property *) current->data);
            strcat(result, propStr);
            strcat(result, "\n");
            free(propStr);
            current = current->next;
        }
    }
    if (cardObj->birthday != NULL) {
        char *bdayStr = dateToString(cardObj->birthday);
        strcat(result, "Birthday: ");
        strcat(result, bdayStr);
        strcat(result, "\n");
        free(bdayStr);
    }
    if (cardObj->anniversary != NULL) {
        char *annivStr = dateToString(cardObj->anniversary);
        strcat(result, "Anniversary: ");
        strcat(result, annivStr);
        strcat(result, "\n");
        free(annivStr);
    }
    return result;
}

// Converts a VCardErrorCode into string for the user.
char* errorToString(VCardErrorCode errCode) {
    const char *msg;
    switch (errCode) {
        case OK:          msg = "OK"; break;
        case INV_FILE:    msg = "Invalid file"; break;
        case INV_CARD:    msg = "Invalid vCard format"; break;
        case INV_PROP:    msg = "Invalid property encountered"; break;
        case INV_DT:      msg = "Invalid date/time format"; break;
        case WRITE_ERROR: msg = "Write error occurred"; break;
        case OTHER_ERROR: msg = "An unspecified error occurred"; break;
        default:          msg = "Unknown error code"; break;
    }
    return strdup(msg);
}


VCardErrorCode writeCard(const char* fileName, const Card* obj){
    char extract[75] = "";
    if(fileName == NULL || obj == NULL){
        return WRITE_ERROR;
    }
        FILE* fptr;
        fptr = fopen(fileName, "w");
        if (fptr == NULL){
            return WRITE_ERROR;
        }
    fprintf(fptr, "BEGIN:VCARD\r\n");
    fprintf(fptr, "VERSION:4.0\r\n");
    //writing fn properties
    if(obj->fn!= NULL){
        
        strcat(extract, obj->fn->name);
    }
    strcat(extract, ":");
    if(obj->fn->values != NULL){
        if(obj->fn->values->head != NULL){
            Node *value = obj->fn->values->head;
            for (Node *value = obj->fn->values->head; value; value = value->next) {
                if (value != obj->fn->values->head) strcat(extract, ",");
                strcat(extract, (char *)value->data);
            }
            
        }
        strcat(extract, "\r\n");
        fprintf(fptr, "%s", extract);

    }
    //optional properties not completed

    // Write birthday 
    if (obj->birthday != NULL) {
        DateTime *dt = obj->birthday;
        if (dt->isText) {
            char *txt = dateToString(dt);
            fprintf(fptr, "BDAY;VALUE=text:%s\r\n", txt);
            free(txt);
        } else {
            if (strlen(dt->date) == 0 && strlen(dt->time) > 0) {
                fprintf(fptr, "BDAY:T%s%s\r\n", dt->time, dt->UTC ? "Z" : "");
            } else if (strlen(dt->time) == 0) {
                fprintf(fptr, "BDAY:%s%s\r\n", dt->date, dt->UTC ? "Z" : "");
            } else {
                fprintf(fptr, "BDAY:%sT%s%s\r\n", dt->date, dt->time, dt->UTC ? "Z" : "");
            }
        }
    }
        // Write anniversary
    if (obj->anniversary != NULL) {
        DateTime *dt = obj->anniversary;
        if (dt->isText) {
            char *txt = dateToString(dt);
            fprintf(fptr, "ANNIVERSARY;VALUE=text:%s\r\n", txt);
            free(txt);
        } else if (strlen(dt->date) == 0 && strlen(dt->time) > 0) {
            fprintf(fptr, "ANNIVERSARY:T%s%s\r\n", dt->time, dt->UTC ? "Z" : "");
        } else if (strlen(dt->time) == 0) {
            fprintf(fptr, "ANNIVERSARY:%s%s\r\n", dt->date, dt->UTC ? "Z" : "");
        } else {
            fprintf(fptr, "ANNIVERSARY:%sT%s%s\r\n", dt->date, dt->time, dt->UTC ? "Z" : "");
        }
    }

    fprintf(fptr, "END:VCARD\r\n");
    fclose(fptr);
    return OK;
}

VCardErrorCode validateCard(const Card* obj) {
    // Validate Card pointer 
    if (obj == NULL){
        return INV_CARD;
    }
    if (obj->fn == NULL){
        return INV_CARD; 
    } 
   
    // Validate optionalProperties list.
    if (obj->optionalProperties == NULL){
        return INV_CARD;
    }

    for (Node *current = obj->optionalProperties->head; current != NULL; current = current->next) {
        Property *prop = (Property *) current->data;

        if (strcmp(prop->name, "VERSION") == 0) {
            return INV_CARD;
        }
        
        if (strcmp(prop->name, "BDAY") == 0 ) {
            return INV_DT;
        }
        if(strcmp(prop->name, "ANNIVERSARY") == 0){
            return INV_DT;
        }
        if (!(strcmp(prop->name, "FN") == 0) ){
            return INV_PROP;
        }
    
        int nprop = 0;
        for (Node *np = prop->values->head; np != NULL; np = np->next){
            nprop++;
        }
        if (nprop != 5){
            return INV_PROP;
        }
        
    }
    
    // Validate the DateTime fields

    if (obj->birthday != NULL) {
        DateTime *datetime = obj->birthday;
        if (datetime == NULL) {
            return INV_DT;
        }
        if (datetime->isText) {
            if (!datetime->text) {
                return INV_DT;
            }
            if (datetime->date && strlen(datetime->date) != 0) {
            return INV_DT;
            }
            if (datetime->time && strlen(datetime->time) != 0) {
            return INV_DT;
            }
            if (datetime->UTC) {
            return INV_DT;
            }
        } 

        else {
            if (!datetime->date || 
                (strlen(datetime->date) != 8 && strlen(datetime->date) != 0)) {
                return INV_DT;
            }
            if (datetime->time && strlen(datetime->time) != 0 && strlen(datetime->time) != 6) {
                return INV_DT;
            }
            if (datetime->text && strlen(datetime->text) != 0){
             return INV_DT;
            }
        }
        
    }
    
    
    return OK;
}
//assignment 3 codes
Card* alternate(){
    Card* obj = malloc(sizeof(Card));
    if (obj == NULL) {
        return NULL;
    }
    obj->fn = NULL;
    obj->optionalProperties = initializeList(propertyToString, deleteProperty, compareProperties);
    obj->birthday = NULL;
    obj->anniversary = NULL;
    return obj;
}

char * getfn(const Card* obj){
    if(obj == NULL || obj->fn == NULL || obj->fn->values == NULL
    || obj->fn->values->head == NULL || obj->fn->values->head->data == NULL){
        return strdup("");
    }
    return strdup((char*) obj->fn->values->head->data);
}
VCardErrorCode setfn(Card* obj, const char* fn) {
    if (obj == NULL || fn == NULL || strlen(fn) == 0) {
        return INV_PROP;
    }
    Property* prop = malloc(sizeof(Property));
    if (prop == NULL) {
        return OTHER_ERROR;
    }
    prop->name = strdup("FN");
    prop->group = strdup("");  
    prop->parameters = initializeList(parameterToString, deleteParameter, compareParameters);
    prop->values = initializeList(valueToString, deleteValue, compareValues);

    insertBack(prop->values, strdup(fn));
    obj->fn = prop;
    return OK;
}

char* getbday(const Card* obj) {
    return obj && obj->birthday ? dateToString(obj->birthday) : strdup("");
}

char* getanniv(const Card* obj) {
    return obj && obj->anniversary ? dateToString(obj->anniversary) : strdup("");
}

//Helper functions
void deleteProperty(void *propPtr) {
    if (propPtr == NULL)
        return;
    Property *prop = (Property *) propPtr;
    if (prop->name)
        free(prop->name);
    if (prop->group)
        free(prop->group);
    if (prop->parameters)
        freeList(prop->parameters);
    if (prop->values)
        freeList(prop->values);
    free(prop);
}

int compareProperties(const void *first, const void *second) {
    if (first == NULL || second == NULL)
        return 0;
    const Property *p1 = (const Property *) first;
    const Property *p2 = (const Property *) second;
    int cmp = strcmp(p1->group, p2->group);
    if (cmp == 0)
        cmp = strcmp(p1->name, p2->name);
    return cmp;
}

char* propertyToString(void *propPtr) {
    if (propPtr == NULL)
        return strdup("null");
    Property *prop = (Property *) propPtr;
    char *buffer = (char *) malloc(1024);
    if (buffer == NULL)
        return NULL;
    buffer[0] = '\0';

    strcat(buffer, "Property: ");
    if (strlen(prop->group) > 0) {
        strcat(buffer, prop->group);
        strcat(buffer, ".");
    }
    strcat(buffer, prop->name);
    strcat(buffer, "\nParameters: ");
    if (prop->parameters && prop->parameters->head) {
        Node *curParam = prop->parameters->head;
        while (curParam != NULL) {
            char *paramStr = parameterToString(curParam->data);
            strcat(buffer, paramStr);
            strcat(buffer, " ");
            free(paramStr);
            curParam = curParam->next;
        }
    } else {
        strcat(buffer, "None");
    }
    strcat(buffer, "\nValues: ");

    // Special formatting for  N 
    if (strcmp(prop->name, "N") == 0) {
        char *family = (prop->values->head && prop->values->head->data) ? (char *)prop->values->head->data : "";
        Node *node = (prop->values->head) ? prop->values->head->next : NULL;
        char *given = (node && node->data) ? (char *)node->data : "";
        node = (node) ? node->next : NULL;
        char *additional = (node && node->data) ? (char *)node->data : "";
        node = (node) ? node->next : NULL;
        char *prefixes = (node && node->data) ? (char *)node->data : "";
        node = (node) ? node->next : NULL;
        char *suffixes = (node && node->data) ? (char *)node->data : "";

        char formattedN[1024];
        if (strlen(suffixes) > 0)
            snprintf(formattedN, sizeof(formattedN), "%s %s %s %s,%s", family, given, additional, prefixes, suffixes);
        else
            snprintf(formattedN, sizeof(formattedN), "%s %s %s %s", family, given, additional, prefixes);
        strcat(buffer, formattedN);
    } else {
        // Default handling for other properties.
        if (prop->values && prop->values->head) {
            Node *curVal = prop->values->head;
            while (curVal != NULL) {
                char *valStr = valueToString(curVal->data);
                strcat(buffer, valStr);
                strcat(buffer, " ");
                free(valStr);
                curVal = curVal->next;
            }
        } else {
            strcat(buffer, "None");
        }
    }
    return buffer;
}


void deleteParameter(void *paramPtr) {
    if (paramPtr == NULL)
        return;
    Parameter *param = (Parameter *) paramPtr;
    if (param->name)
        free(param->name);
    if (param->value)
        free(param->value);
    free(param);
}


int compareParameters(const void *first, const void *second) {
    if (first == NULL || second == NULL)
        return 0;
    const Parameter *p1 = (const Parameter *) first;
    const Parameter *p2 = (const Parameter *) second;
    return strcmp(p1->name, p2->name);
}


char* parameterToString(void *paramPtr) {
    if (paramPtr == NULL)
        return strdup("null");
    Parameter *param = (Parameter *) paramPtr;
    char *buffer = (char *) malloc(256);
    if (buffer == NULL)
        return NULL;
    snprintf(buffer, 256, "%s=%s", param->name, param->value);
    return buffer;
}


void deleteValue(void *valPtr) {
    if (valPtr)
        free(valPtr);
}


int compareValues(const void *first, const void *second) {
    if (first == NULL || second == NULL)
        return 0;
    return strcmp((const char *) first, (const char *) second);
}


char* valueToString(void *valPtr) {
    if (valPtr == NULL)
        return strdup("null");
    return strdup((char *) valPtr);
}


void deleteDate(void *datePtr) {
    if (datePtr == NULL)
        return;
    DateTime *datetime = (DateTime *) datePtr;
    if (datetime->date)
        free(datetime->date);
    if (datetime->time)
        free(datetime->time);
    if (datetime->text)
        free(datetime->text);
    free(datetime);
}


int compareDates(const void *first, const void *second) {
    (void) first;
    (void) second;
    return 0;
}

char* dateToString(void *datePtr) {
    if (datePtr == NULL){
        return strdup("");
    }

    DateTime *datetime = (DateTime *) datePtr;
    char *extract = (char *) malloc(256);
    if (datetime->isText) {
        snprintf(extract, 256, "%s", datetime->text);
    } else {
        if (strlen(datetime->time) >= 6) {
            snprintf(extract, 256,
                     "%4.4s-%2.2s-%2.2s %2.2s:%2.2s:%2.2s",
                     datetime->date,
                     datetime->date + 4,
                     datetime->date + 6,
                     datetime->time,
                     datetime->time + 2,
                     datetime->time + 4);
        } else {
            snprintf(extract, 256,
                     "%4.4s-%2.2s-%2.2s",
                     datetime->date,
                     datetime->date + 4,
                     datetime->date + 6);
        }
    }
    return extract;
}


