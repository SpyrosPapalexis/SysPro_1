struct Node{
    int item;
    Node* next;
    Node(int);
};

struct Queue{
    Node *start, *end;
    Queue();
    void push(int);
    int pop();
    bool isempty();
    ~Queue();
};