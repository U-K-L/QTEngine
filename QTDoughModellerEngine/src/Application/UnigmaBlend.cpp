#include "UnigmaBlend.h"


#define DATA_READY_EVENT_NAME "Local\\DataReadyEvent"
#define READ_COMPLETE_EVENT_NAME "Local\\ReadCompleteEvent"
#define SHARED_MEMORY_NAME "Local\\MySharedMemory"
#define NUM_OBJECTS 10
RenderObject* renderObjects = nullptr;
//Return types:
// 0: Success
// -1: Error (general)
// 1: No File
// 2: Data not ready.
void PrintRenderObjectRaw(const RenderObject& obj) {
    std::cout << "RenderObject Name: " << obj.name << "\n";
    std::cout << "ID: " << obj.id << "\n";
    std::cout << "Vertex Count: " << obj.vertexCount << "\n";
    std::cout << "Index Count: " << obj.indexCount << "\n";

    std::cout << "Transform Matrix:\n";
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << std::setw(10) << obj.transformMatrix[i*4 + j] << " ";
        }
        std::cout << "\n";
    }

    std::cout << "Vertices:\n";
    for (int i = 0; i < obj.vertexCount; i++) {
        std::cout << "  Vertex " << i << ": ("
            << obj.vertices[i].x << ", "
            << obj.vertices[i].y << ", "
            << obj.vertices[i].z << ")\n";
    }

    std::cout << "Address of obj.indices: " << static_cast<const void*>(obj.indices) << std::endl;
    std::cout << "Indices:\n";
    for (int i = 0; i < obj.indexCount; i++) {
        std::cout << " RAW PRINT Index " << i << ": " << obj.indices[i] << "\n";
    }
}

int GatherBlenderInfo()
{
    // Attempt to open the events
    HANDLE hDataReadyEvent = OpenEventA(
        SYNCHRONIZE,             // Desired access
        FALSE,                   // Do not inherit handle
        DATA_READY_EVENT_NAME    // Name of the event
    );
    HANDLE hReadCompleteEvent = OpenEventA(
        EVENT_MODIFY_STATE,      // Desired access
        FALSE,                   // Do not inherit handle
        READ_COMPLETE_EVENT_NAME // Name of the event
    );

    if (hDataReadyEvent == NULL || hReadCompleteEvent == NULL) {
        // Events not available; exit the function
        return 1;
    }



    const size_t sharedMemorySize = sizeof(RenderObject) * NUM_OBJECTS;
    // Open the file mapping object
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,   // Read/write access
        FALSE,                 // Do not inherit the name
        SHARED_MEMORY_NAME       // Name of mapping object
    );

    if (hMapFile == NULL) {
        CloseHandle(hDataReadyEvent);
        CloseHandle(hReadCompleteEvent);
        return 1;
    }

    // Wait for the data to be ready.
    DWORD waitResult = WaitForSingleObject(hDataReadyEvent, 0);
    if (waitResult != WAIT_OBJECT_0) {
        // Failed to wait for data ready; clean up and exit
        CloseHandle(hMapFile);
        CloseHandle(hDataReadyEvent);
        CloseHandle(hReadCompleteEvent);
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
        CloseHandle(hDataReadyEvent);
        CloseHandle(hReadCompleteEvent);
        return 1;
    }


    // Cast the mapped view to an array of GameObject structs
    renderObjects = static_cast<RenderObject*>(pBuf);

    // Iterate over each GameObject and print the data
    for (int i = 0; i < NUM_OBJECTS; ++i) {
        std::cout << "Reader: GameObject Name " << renderObjects[i].name << std::endl;
        std::cout << "Reader: GameObject[" << i << "].id = " << renderObjects[i].id << std::endl;

        // Print the transformMatrix
        std::cout << "Reader: GameObject[" << i << "].transformMatrix =\n";
        for (int row = 0; row < 4; ++row) {
            std::cout << "[ ";
            for (int col = 0; col < 4; ++col) {
                std::cout << renderObjects[i].transformMatrix[row*4 + col] << " ";
            }
            std::cout << "]\n";
        }

        std::cout << "Vertices" << std::endl;
        int vertCount = renderObjects[i].vertexCount;
        for (int j = 0; j < vertCount; j++)
        {
            std::cout << "(" << renderObjects[i].vertices[j].x;
            std::cout << "," << renderObjects[i].vertices[j].y;
            std::cout << "," << renderObjects[i].vertices[j].z << ")" << std::endl;
        }

        std::cout << "Indices TRUE VALUE" << std::endl;
        int indicesCount = renderObjects[i].indexCount;
        for (int j = 0; j < indicesCount; j++)
        {
            std::cout << " RAW " << renderObjects[i].indices[j] << std::endl;
        }

        std::cout << " Print after debug " << std::endl;
        PrintRenderObjectRaw(renderObjects[i]);
        for (int j = 0; j < indicesCount; j++)
        {
            std::cout << " RAW " << renderObjects[i].indices[j] << std::endl;
        }
    }


    // Signal that reading is complete
    SetEvent(hReadCompleteEvent);
    ResetEvent(hDataReadyEvent);

    // Clean up
    //UnmapViewOfFile(pBuf);
    //CloseHandle(hMapFile);
    //CloseHandle(hDataReadyEvent);
    //CloseHandle(hReadCompleteEvent);


    return 0;
}