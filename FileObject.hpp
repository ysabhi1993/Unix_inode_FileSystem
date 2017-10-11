#include <cstring>
#include <climits>
#include <fstream>
#include <iostream>

using namespace std;

class FileObject {
        private:
            double key;
            long fileIndex;
            string dataString;

        public:
            static long objectCount;

        public:
            FileObject(double _key, string _dataString);
            FileObject(double _key, long _fileIndex);
            // Return the key of the object
            double getKey() { return key; }

            // Return the string
            string getDataString() { return dataString; }

            // Return the fileIndex
            long getFileIndex() { return fileIndex; }
};
