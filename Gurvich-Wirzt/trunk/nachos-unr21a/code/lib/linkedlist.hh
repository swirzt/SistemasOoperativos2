#ifndef LINKED_LIST
#define LINKED_LIST

// import io
#include <iostream>

// make a template for 2 types of data
template <class Key, class Value>
class Node
{
public:
    Node<Key, Value>(Key key, Value value);

    Key key;
    Value value; // Si un dia tenemos archivos muy grandes su sector puede llegar a ser negativo al convertirse a int
    Node<Key, Value> *next;
};

template <class Key, class Value>
Node<Key, Value>::Node(Key k, Value val)
{
    this->key = k;
    this->value = val;
    this->next = nullptr;
}

template <class Key, class Value>
class LinkedList
{
public:
    LinkedList(bool (*f)(Key, Key));
    ~LinkedList();
    void insert(Key key, Value value);
    void remove(Key key);
    bool get(Key key, Value *value);
    void update(Key key, Value value);
    void print();

private:
    bool (*f)(Key k1, Key k2);
    Node<Key, Value> *head;
};

template <class Key, class Value>
LinkedList<Key, Value>::LinkedList(bool (*func)(Key, Key))
{
    f = func;
    head = nullptr;
}

// Delete linked list
template <class Key, class Value>
LinkedList<Key, Value>::~LinkedList()
{
    Node<Key, Value> *current = head;
    while (current != nullptr)
    {
        Node<Key, Value> *temp = current;
        current = current->next;
        delete temp;
    }
}

template <class Key, class Value>
void LinkedList<Key, Value>::insert(Key key, Value value)
{
    Node<Key, Value> *newNode = new Node<Key, Value>(key, value);
    newNode->next = head;
    head = newNode;
}

template <class Key, class Value>
void LinkedList<Key, Value>::remove(Key key)
{
    Node<Key, Value> *current = head;
    Node<Key, Value> *prev = nullptr;
    while (current != nullptr)
    {
        if (f(current->key, key))
        {
            if (prev == nullptr)
            {
                head = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            delete current;
            return;
        }
        prev = current;
        current = current->next;
    }
}

template <class Key, class Value>
bool LinkedList<Key, Value>::get(Key key, Value *val)
{
    Node<Key, Value> *current = head;
    while (current != nullptr)
    {
        if (f(current->key, key))
        {
            *val = current->value;
            return true;
        }
        current = current->next;
    }
    return false;
}

template <class Key, class Value>
void LinkedList<Key, Value>::update(Key key, Value value)
{
    Node<Key, Value> *current = head;
    while (current != nullptr)
    {
        if (f(current->key, key))
        {
            current->value = value;
            return;
        }
        current = current->next;
    }
}

template <class Key, class Value>
void LinkedList<Key, Value>::print()
{
    Node<Key, Value> *current = head;
    while (current != nullptr)
    {
        std::cout << current->key << " " << current->value << std::endl;
        current = current->next;
    }
}

#endif