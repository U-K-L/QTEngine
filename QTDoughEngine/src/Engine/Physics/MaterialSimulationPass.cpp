#include "MaterialSimulationPass.h"

MaterialSimulation* MaterialSimulation::instance = nullptr;

MaterialSimulation::MaterialSimulation()
{

}

MaterialSimulation::~MaterialSimulation()
{

}

void MaterialSimulation::InitMaterialSim()
{
	Field.FieldSize = glm::ivec3(64, 64, 16);
	InitQuanta();
}

void MaterialSimulation::InitQuanta()
{
	QTDoughApplication* app = QTDoughApplication::instance;
	quantaMemorySize = sizeof(Quanta) * QUANTA_COUNT;
	std::cout << "Required size for Quanta is: " << quantaMemorySize << std::endl;
	Field.Quantas = (Quanta*)malloc(quantaMemorySize);
	InitQuantaPositions();

	//Allocate on the GPU.
	//Make staging buffers.
	VkBuffer quantaStagingBuffer;
	VkDeviceMemory quantaStagingMemory;
	app->CreateBuffer(quantaMemorySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		quantaStagingBuffer, quantaStagingMemory);

	//Map quanta data to staging buffer.
	void* quantaData;
	vkMapMemory(app->_logicalDevice, quantaStagingMemory, 0, quantaMemorySize, 0, &quantaData);
	memcpy(quantaData, Field.Quantas, quantaMemorySize);
	vkUnmapMemory(app->_logicalDevice, quantaStagingMemory);

	//Create device local buffer for quanta.
	QuantaStorageBuffers.resize(3); //Triple buffering. In, Out, and READ.
	QuantaStorageMemory.resize(3);

	for (int i = 0; i < QuantaStorageBuffers.size(); i++)
	{
		app->CreateBuffer(quantaMemorySize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, QuantaStorageBuffers[i], QuantaStorageMemory[i]);
		app->CopyBuffer(quantaStagingBuffer, QuantaStorageBuffers[i], quantaMemorySize);
	}
}

void MaterialSimulation::InitQuantaPositions()
{
	Quanta* Quantas = Field.Quantas;
	glm::vec3 fs = glm::vec3(Field.FieldSize);
	double volume = (double)fs.x * (double)fs.y * (double)fs.z;
	double step = std::cbrt(volume / (double)QUANTA_COUNT);

	int nx = std::max(1, (int)std::round((double)fs.x / step));
	int ny = std::max(1, (int)std::round((double)fs.y / step));
	int nz = std::max(1, (int)std::round((double)fs.z / step));

	glm::vec3 halfSize = fs * 0.5f;
	float dx = fs.x / (float)nx;
	float dy = fs.y / (float)ny;
	float dz = fs.z / (float)nz;

	uint64_t index = 0;
	for (int z = 0; z < nz && index < QUANTA_COUNT; z++)
	{
		for (int y = 0; y < ny && index < QUANTA_COUNT; y++)
		{
			for (int x = 0; x < nx && index < QUANTA_COUNT; x++)
			{
				float px = (x + 0.5f) * dx - halfSize.x;
				float py = (y + 0.5f) * dy - halfSize.y;
				float pz = (z + 0.5f) * dz - halfSize.z;

				Quantas[index].position = glm::vec4(px, py, pz, 1.0f);
				Quantas[index].resonance = glm::vec4(0.0f);
				Quantas[index].information = glm::ivec4(0);
				Quantas[index].mana = glm::vec4(0.0f);
				index++;
			}
		}
	}

	for (; index < QUANTA_COUNT; index++)
	{
		Quantas[index].position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		Quantas[index].resonance = glm::vec4(0.0f);
		Quantas[index].information = glm::ivec4(0);
		Quantas[index].mana = glm::vec4(0.0f);
	}

	std::cout << "Quanta initialized: " << nx << "x" << ny << "x" << nz
		<< " (" << nx * ny * nz << " grid points, step=" << step << "m)" << std::endl;
}

void MaterialSimulation::SerializeQuantaBlob(const std::string& path)
{
	std::filesystem::path dir = std::filesystem::path(path).parent_path();
	if (!dir.empty())
		std::filesystem::create_directories(dir);
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << "Failed to open blob file for writing: " << path << std::endl;
		return;
	}

	// Header: field size + quanta count.
	uint64_t count = QUANTA_COUNT;
	file.write(reinterpret_cast<const char*>(&Field.FieldSize), sizeof(glm::ivec3));
	file.write(reinterpret_cast<const char*>(&count), sizeof(uint64_t));
	file.write(reinterpret_cast<const char*>(Field.Quantas), sizeof(Quanta) * QUANTA_COUNT);

	file.close();
	std::cout << "Quanta blob serialized to: " << path << " (" << sizeof(Quanta) * QUANTA_COUNT << " bytes)" << std::endl;
}

void MaterialSimulation::SerializeQuantaText(const std::string& path)
{
	std::filesystem::path dir = std::filesystem::path(path).parent_path();
	if (!dir.empty())
		std::filesystem::create_directories(dir);
	std::ofstream file(path);
	if (!file.is_open())
	{
		std::cerr << "Failed to open text file for writing: " << path << std::endl;
		return;
	}

	file << "FieldSize: " << Field.FieldSize.x << " " << Field.FieldSize.y << " " << Field.FieldSize.z << "\n";
	file << "QuantaCount: " << QUANTA_COUNT << "\n";

	Quanta* Quantas = Field.Quantas;
	for (uint64_t i = 0; i < QUANTA_COUNT; i++)
	{
		const Quanta& q = Quantas[i];
		file << "[" << i << "] "
			<< "pos(" << q.position.x << " " << q.position.y << " " << q.position.z << " " << q.position.w << ") "
			<< "res(" << q.resonance.x << " " << q.resonance.y << " " << q.resonance.z << " " << q.resonance.w << ") "
			<< "inf(" << q.information.x << " " << q.information.y << " " << q.information.z << " " << q.information.w << ") "
			<< "mana(" << q.mana.x << " " << q.mana.y << " " << q.mana.z << " " << q.mana.w << ")\n";
	}

	file.close();
	std::cout << "Quanta text serialized to: " << path << std::endl;
}

void MaterialSimulation::DeserializeQuantaBlob(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << "Failed to open blob file for reading: " << path << std::endl;
		return;
	}

	glm::ivec3 loadedFieldSize;
	uint64_t loadedCount;
	file.read(reinterpret_cast<char*>(&loadedFieldSize), sizeof(glm::ivec3));
	file.read(reinterpret_cast<char*>(&loadedCount), sizeof(uint64_t));

	if (loadedCount != QUANTA_COUNT)
	{
		std::cerr << "Blob quanta count mismatch: file has " << loadedCount
			<< " but expected " << QUANTA_COUNT << std::endl;
		file.close();
		return;
	}

	Field.FieldSize = loadedFieldSize;
	file.read(reinterpret_cast<char*>(Field.Quantas), sizeof(Quanta) * QUANTA_COUNT);

	file.close();
	std::cout << "Quanta blob deserialized from: " << path
		<< " (field " << Field.FieldSize.x << "x" << Field.FieldSize.y << "x" << Field.FieldSize.z << ")" << std::endl;
}