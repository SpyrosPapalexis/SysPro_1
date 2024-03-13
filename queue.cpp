#include <iostream>
#include "queue.hpp"

Node::Node(int i){
    item = i;
    next = NULL;
}

Queue::Queue(){
    start = end = NULL;
}

    void Queue::push(int i){
        Node* t = new Node(i);
 
        if (end == NULL){
            start = end = t;
            return;
        }
 
        end->next = t;
        end = t;
    }
 
    int Queue::pop(){
        if (start == NULL) return 0;
 
        Node* t = start;
        start = start->next;
 
        if (start == NULL) end = NULL;
        int i = t->item;
        delete (t);
        return i;
    }

    bool Queue::isempty(){
        if (start == NULL) return true;
        return false;
    }

Queue::~Queue(){
    while (!isempty()){
        pop();
    }
}
