#include "UnigmaBlend.h"
#include "../Engine/Core/UnigmaGameObject.h"

#define SHARED_MEMORY_NAME "Local\\MySharedMemory"
#define NUM_OBJECTS 10

int GatherBlenderInfo()
{
    const size_t sharedMemorySize = sizeof(UnigmaGameObject) * NUM_OBJECTS;
    // Open the file mapping object
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,   // Read/write access
        FALSE,                 // Do not inherit the name
        SHARED_MEMORY_NAME       // Name of mapping object
    );

    if (hMapFile == NULL) {
        return 1;
    }

    // Map a view of the file into the address space of the calling process
    LPVOID pBuf = MapViewOfFile(
        hMapFile,            // Handle to map object
        FILE_MAP_ALL_ACCESS, // Read/write permission
        0,                   // File offset high
        0,                   // File offset low
        sharedMemorySize          // Number of bytes to map
    );

    if (pBuf == NULL) {
        CloseHandle(hMapFile);
        return 1;
    }


    // Cast the mapped view to an array of GameObject structs
    UnigmaGameObject* gameObjects = static_cast<UnigmaGameObject*>(pBuf);

    // Iterate over each GameObject and print the data
    for (int i = 0; i < NUM_OBJECTS; ++i) {
        std::cout << "Reader: GameObject Name " << gameObjects[i].name << std::endl;
        std::cout << "Reader: GameObject[" << i << "].id = " << gameObjects[i].id << std::endl;

        // Print the transformMatrix
        std::cout << "Reader: GameObject[" << i << "].transformMatrix =\n";
        for (int row = 0; row < 4; ++row) {
            std::cout << "[ ";
            for (int col = 0; col < 4; ++col) {
                std::cout << gameObjects[i].transformMatrix[row][col] << " ";
            }
            std::cout << "]\n";
        }
    }


    // Clean up
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);

    return 0;
}