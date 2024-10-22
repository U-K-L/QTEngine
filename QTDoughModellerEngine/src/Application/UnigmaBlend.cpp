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