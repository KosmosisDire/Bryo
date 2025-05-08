#include <string.h>
#include <stdio.h>



typedef struct Element
{
    float x, y;
    float width, height;
} Element;

typedef struct Box
{
    Element base;

} Box;


void mui_begin(void* el)
{
    Box* box = (Box*)el;
    printf("mui_begin(%p) %f %f\n", el, box->base.width, box->base.height);
}

void mui_end()
{
    printf("mui_end()\n");
}