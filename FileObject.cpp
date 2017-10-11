#include "FileObject.hpp"
#include <cstring>
#include <climits>
#include <fstream>
#include <iostream>

#define OBJECT_FILE "objects/objectFile"

FileObject::FileObject(double _key, string _dataString) : key(_key), dataString(_dataString) {
    fileIndex = objectCount++;

    // Open a file and write the string to it
    ofstream ofile(OBJECT_FILE, ios::app);
    ofile << dataString << endl;
    ofile.close();
}

FileObject::FileObject(double _key, long _fileIndex) : key(_key), fileIndex(_fileIndex) {
    // Open a file and read the dataString
    ifstream ifile(OBJECT_FILE);
    for (long i = 0; i < fileIndex + 1; ++i) {
        getline(ifile, dataString);
    }
    ifile.close();
}

long FileObject::objectCount = 0;