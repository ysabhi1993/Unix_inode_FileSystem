#include "B+tree.hpp"

//Config parameters
#define CONFIG_FILE "./bplustree.config"
#define SESSION_FILE "./.tree.session"

// Constants
#define TREE_PREFIX "leaves/leaf_"
#define OBJECT_FILE "objects/objectFile"
#define DEFAULT_LOCATION -1

#include <sys/types.h>
#include <algorithm>
#include <climits>
#include <cmath> 
#include <string>
#include <iostream>
#include <memory>
#include <queue>
#include <vector>

using std::list
using std::sort
using std::vector
using std::queue

long TreeNode::lowerBound = 0;
long TreeNode::upperBound = 0;
long TreeNode::pageSize = 0;
long TreeNode::fileCount = 0;

TreeNode *bRoot = nullptr;

//Initialize a B+ Tree
TreeNode::TreeNode(){
    //Initially all the fileNames are DEFAULT_LOCATION
    parentIndex = DEFAULT_LOCATION;
    nextLeafIndex = DEFAULT_LOCATION;
    previousLeafIndex = DEFAULT_LOCATION;

    // Initially every node is a leaf
    leaf = true; 

    // Exit if the lowerBoundKey is not defined
    if (lowerBound == 0) {
        cout << "LowerKeyBound not defined";
        exit(1); 
    }
    // LeafNode properties
    fileIndex = ++fileCount; 
}

Node::Node(long _fileIndex) {
    // Exit if the lowerBoundKey is not defined
    if (lowerBound == 0) {
        cout << "LowerKeyBound not defined";
        exit(1);
    }

    // Load the current node from disk
    fileIndex = _fileIndex;
    readFromDisk();
}

void Node::initialize() {
    // Set page size
    ifstream configFile;
    configFile.open(CONFIG_FILE);
    configFile >> pageSize;

    // Save some place in the file for the header
    long headerSize = sizeof(fileIndex)
        + sizeof(leaf)
        + sizeof(parentIndex)
        + sizeof(nextLeafIndex)
        + sizeof(previousLeafIndex);
    pageSize = pageSize - headerSize;

    // Compute parameters
    long nodeSize = sizeof(fileIndex);
    long keySize = sizeof(keyType);
    lowerBound = floor((pageSize - nodeSize) / (2 * (keySize + nodeSize)));
    upperBound = 2 * lowerBound;
    pageSize = pageSize + headerSize;
}

//Check where the given key fits in the Keys vector.
long Node::getKeyPosition(double key) {
    // If keys are empty, return
    if (keys.size() == 0 || key <= keys.front()) {
        return 0;
    }

    for (long i = 1; i < (long)keys.size(); ++i) {
        if (keys[i -1] < key && key <= keys[i]) {
            return i;
        }
    }

    return keys.size();
}

//Write to disk, information of all the files.
void Node::commitToDisk() {
    // Create a character buffer which will be written to disk
    long location = 0;
    char buffer[pageSize];

    memcpy(buffer + location, &fileIndex, sizeof(fileIndex)); // Store the fileIndex
    location += sizeof(fileIndex);

    memcpy(buffer + location, &leaf, sizeof(leaf)); // Add the leaf to memory
    location += sizeof(leaf);

    memcpy(buffer + location, &parentIndex, sizeof(parentIndex)); // Add parent to memory
    location += sizeof(parentIndex);

    memcpy(buffer + location, &previousLeafIndex, sizeof(nextLeafIndex)); // Add the previous leaf node
    location += sizeof(nextLeafIndex);
  
    memcpy(buffer + location, &nextLeafIndex, sizeof(nextLeafIndex)); // Add the next leaf node
    location += sizeof(nextLeafIndex);

    long numKeys = keys.size(); // Store the number of keys
    memcpy(buffer + location, &numKeys, sizeof(numKeys));
    location += sizeof(numKeys);

    // Add the keys to memory
    for (auto key : keys) {
        memcpy(buffer + location, &key, sizeof(key));
        location += sizeof(key);
    }

    // Add the child pointers to memory
    if (!leaf) {
        for (auto childIndex : childIndices) {
            memcpy(buffer + location, &childIndex, sizeof(childIndex));
            location += sizeof(childIndex);
        }
    } else {
        for (auto objectPointer : objectPointers) {
            memcpy(buffer + location, &objectPointer, sizeof(objectPointer));
            location += sizeof(objectPointer);
        }
    }

    // Create a binary file and write to memory
    ofstream nodeFile;
    nodeFile.open(getFileName(), ios::binary|ios::out);
    nodeFile.write(buffer, pageSize);
    nodeFile.close();
}

void Node::readFromDisk() {
    // Create a character buffer which will be written to disk
    long location = 0;
    char buffer[pageSize];

    // Open the binary file ane read into memory
    ifstream nodeFile;
    nodeFile.open(getFileName(), ios::binary|ios::in);
    nodeFile.read(buffer, pageSize);
    nodeFile.close();

    memcpy((char *) &fileIndex, buffer + location, sizeof(fileIndex)); // Retrieve the fileIndex
    location += sizeof(fileIndex);

    memcpy((char *) &leaf, buffer + location, sizeof(leaf)); // Retreive the type of node
    location += sizeof(leaf);

    
    memcpy((char *) &parentIndex, buffer + location, sizeof(parentIndex)); // Retrieve the parentIndex
    location += sizeof(parentIndex);

    memcpy((char *) &previousLeafIndex, buffer + location, sizeof(previousLeafIndex)); // Retrieve the previousLeafIndex
    location += sizeof(previousLeafIndex);

    memcpy((char *) &nextLeafIndex, buffer + location, sizeof(nextLeafIndex)); // Retrieve the nextLeafIndex
    location += sizeof(nextLeafIndex);

    long numKeys;
    memcpy((char *) &numKeys, buffer + location, sizeof(numKeys)); // Retrieve the number of keys
    location += sizeof(numKeys);

    // Retrieve the keys
    keys.clear(); 
    double key;
    for (long i = 0; i < numKeys; ++i) {
        memcpy((char *) &key, buffer + location, sizeof(key));
        location += sizeof(key);
        keys.push_back(key);
    }

    // Retrieve childPointers
    if (!leaf) {
        childIndices.clear();
        long childIndex;
        for (long i = 0; i < numKeys + 1; ++i) {
            memcpy((char *) &childIndex, buffer + location, sizeof(childIndex));
            location += sizeof(childIndex);
            childIndices.push_back(childIndex);
        }
    } else {
        objectPointers.clear();
        long objectPointer;
        for (long i = 0; i < numKeys; ++i) {
            memcpy((char *) &objectPointer, buffer + location, sizeof(objectPointer));
            location += sizeof(objectPointer);
            objectPointers.push_back(objectPointer);
        }
    }
}

/*
//Helper for object Insertion
void Node::insertObject(DBObject object) {
    long position = getKeyPosition(object.getKey());

    // insert the new key to keys
    keys.insert(keys.begin() + position, object.getKey());

    // insert the object pointer to the end
    objectPointers.insert(objectPointers.begin() + position, object.getFileIndex());

    // Commit the new node back into memory
    commitToDisk();
}
*/

//Split a node if it is full
void Node::splitInternal() {
    //Create a surrogate internal node
    Node *surrogateInternalNode = new Node();
    surrogateInternalNode->setToInternalNode();

    //Fix the keys of the new node
    double startPoint = *(keys.begin() + lowerBound);
    for (auto key = keys.begin() + lowerBound + 1; key != keys.end(); ++key) {
        surrogateInternalNode->keys.push_back(*key);
    }

    //Resize the keys of the current node
    keys.resize(lowerBound);

    //Partition children for the surrogateInternalNode
    for (auto childIndex = childIndices.begin() + lowerBound + 1; childIndex != childIndices.end(); ++childIndex) {
         surrogateInternalNode->childIndices.push_back(*childIndex);

        //Assign parent to the children nodes
        Node *tempChildNode = new Node(*childIndex);
        tempChildNode->parentIndex = surrogateInternalNode->fileIndex;
        tempChildNode->commitToDisk();
        delete tempChildNode;
    }

    //Fix children for the current node
    childIndices.resize(lowerBound + 1);

    //If the current node is not a root node
    if (parentIndex != DEFAULT_LOCATION) {
        //Assign parents
        surrogateInternalNode->parentIndex = parentIndex;
        surrogateInternalNode->commitToDisk();
        commitToDisk();

        //Now we push up the splitting one level
        Node *tempParent = new Node(parentIndex);
        tempParent->insertNode(startPoint, fileIndex, surrogateInternalNode->fileIndex);
        delete tempParent;
    } else {
        //Create a new parent node
        Node *newParent = new Node();
        newParent->setToInternalNode();

        //Assign parents
        surrogateInternalNode->parentIndex = newParent->fileIndex;
        parentIndex = newParent->fileIndex;

        //Insert the key into the keys
        newParent->keys.push_back(startPoint);

        //Insert the children
        newParent->childIndices.push_back(fileIndex);
        newParent->childIndices.push_back(surrogateInternalNode->fileIndex);

        // Commit changes to disk
        newParent->commitToDisk();
        commitToDisk();
        surrogateInternalNode->commitToDisk();

        // Clean up the previous root node
        delete bRoot;

        // Reset the root node
        bRoot = newParent;
    }

    // Clean the surrogateInternalNode
    delete surrogateInternalNode;
}

void Node::serialize() {
    //Return if node is empty
    if (keys.size() == 0) {
        return;
    }

    queue< pair<long, char> > previousLevel;
    previousLevel.push(make_pair(fileIndex, 'N'));

    long currentIndex;
    Node *iterator;
    char type;
    while (!previousLevel.empty()) {
        queue< pair<long, char> > nextLevel;

        while (!previousLevel.empty()) {
            //Get the front and pop
            currentIndex = previousLevel.front().first;
            iterator = new Node(currentIndex);
            type = previousLevel.front().second;
            previousLevel.pop();

            //If it a seperator, print and move ahead
            if (type == '|') {
                cout << "|| ";
                continue;
            }

            //Print all the keys
            for (auto key : iterator->keys) {
                cout << key << " ";
            }

            // Enqueue all the children
            for (auto childIndex : iterator->childIndices) {
                nextLevel.push(make_pair(childIndex, 'N'));

                // Insert a marker to indicate end of child
                nextLevel.push(make_pair(DEFAULT_LOCATION, '|'));
            }

            // Delete allocated memory
            delete iterator;
        }

        // Seperate different levels
        cout << endl << endl;
        previousLevel = nextLevel;
    }
}

void Node::insertNode(double key, long leftChildIndex, long rightChildIndex) {
    // insert the new key to keys
    long position = getKeyPosition(key);
    keys.insert(keys.begin() + position, key);

    // insert the newChild
    childIndices.insert(childIndices.begin() + position + 1, rightChildIndex);

    // commit changes to disk
    commitToDisk();

    // If this overflows, we move again upward
    if ((long)keys.size() > upperBound) {
        splitInternal();
    }

    // Update the root if the element was inserted in the root
    if (fileIndex == bRoot->getFileIndex()) {
        bRoot->readFromDisk();
    }
}

void Node::splitLeaf() {
    // Create a surrogate leaf node with the keys and object Pointers
    Node *surrogateLeafNode = new Node();
    for (long i = lowerBound; i < (long) keys.size(); ++i) {
        DBObject object = DBObject(keys[i], objectPointers[i]);
        surrogateLeafNode->insertObject(object);
    }

    // Resize the current leaf node and commit the node to disk
    keys.resize(lowerBound);
    objectPointers.resize(lowerBound);

    // Link up the leaves
    long tempLeafIndex = nextLeafIndex;
    nextLeafIndex = surrogateLeafNode->fileIndex;
    surrogateLeafNode->nextLeafIndex = tempLeafIndex;

    // If the tempLeafIndex is not null we have to load it and set its
    // previous index
    if (tempLeafIndex != DEFAULT_LOCATION) {
        Node *tempLeaf = new Node(tempLeafIndex);
        tempLeaf->previousLeafIndex = surrogateLeafNode->fileIndex;
        tempLeaf->commitToDisk();
        delete tempLeaf;
    }

    surrogateLeafNode->previousLeafIndex = fileIndex;

    // Consider the case when the current node is not a root
    if (parentIndex != DEFAULT_LOCATION) {
        // Assign parents
        surrogateLeafNode->parentIndex = parentIndex;
        surrogateLeafNode->commitToDisk();
        commitToDisk();

        // Now we push up the splitting one level
        Node *tempParent = new Node(parentIndex);
        tempParent->insertNode(surrogateLeafNode->keys.front(), fileIndex, surrogateLeafNode->fileIndex);
        delete tempParent;
    } else {
        // Create a new parent node
        Node *newParent = new Node();
        newParent->setToInternalNode();

        // Assign parents
        surrogateLeafNode->parentIndex = newParent->fileIndex;
        parentIndex = newParent->fileIndex;

        // Insert the key into the keys
        newParent->keys.push_back(surrogateLeafNode->keys.front());

        // Insert the children
        newParent->childIndices.push_back(this->fileIndex);
        newParent->childIndices.push_back(surrogateLeafNode->fileIndex);

        // Commit to disk
        newParent->commitToDisk();
        surrogateLeafNode->commitToDisk();
        commitToDisk();

        // Clean up the root node
        delete bRoot;

        // Reset the root node
        bRoot = newParent;
    }

    // Clean up surrogateNode
    delete surrogateLeafNode;
}

void insert(Node *root, DBObject object) {
    //If the root is a leaf, we can directly insert
    if (root->isLeaf()) {
        //Insert object
        root->insertObject(object);

        //Split if required
        if (root->size() > root->upperBound) {
            root->splitLeaf();
        }
    } else {
        //We traverse the tree
        long position = root->getKeyPosition(object.getKey());

        //Load the node from disk
        Node *nextRoot = new Node(root->childIndices[position]);

        //Recurse into the node
        insert(nextRoot, object);

        //Clean up
        delete nextRoot;
    }
}

//Point search in a BPlusTree
void pointQuery(Node *root, double searchKey) {
    //If the root is a leaf, we can directly search
    if (root->isLeaf()) {
        //Print all nodes in the current leaf
        for (long i = 0; i < (long) root->keys.size(); ++i) {
            if (root->keys[i] == searchKey) {
#ifdef DEBUG_NORMAL
                cout << root->keys[i] << " ";
#endif
#ifdef OUTPUT
                cout << DBObject(root->keys[i], root->objectPointers[i]).getDataString() << endl;
#endif
            }
        }

        //Check nextleaf for same node
        if (root->nextLeafIndex != DEFAULT_LOCATION) {
            //Load up the nextLeaf from disk
            Node *tempNode = new Node(root->nextLeafIndex);

            //Check in the nextLeaf and delegate
            if (tempNode->keys.front() == searchKey) {
                pointQuery(tempNode, searchKey);
            }
            delete tempNode;
        }
    } else {
        //We traverse the tree
        long position = root->getKeyPosition(searchKey);

        //Load the node from disk
        Node *nextRoot = new Node(root->childIndices[position]);

        //Recurse into the node
        pointQuery(nextRoot, searchKey);

        //Clean up
        delete nextRoot;
    }
}





