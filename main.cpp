#include <QCoreApplication>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <sstream>
#include <random>
#include <chrono>

struct ListNode { // ListNode модифицировать нельзя
    ListNode* prev = nullptr; // указатель на предыдущий элемент или nullptr
    ListNode* next = nullptr;
    ListNode* rand = nullptr; // указатель на произвольный элемент данного списка, либо `nullptr`
    std::string data; // произвольные пользовательские данные
};

struct NewListNode
{
    ListNode* node;
    int randIndex;
};

const int maxNodeCount = 1000000;
const int minWordLen = 5;
const int maxWordLen = 50;

// создание файла input.in
void createInputFile(const std::string& filename, int nodeCount)
{
    std::ofstream out(filename);
    if (!out)
        throw std::runtime_error("cannot create file");

    const std::string chars =
        "abcdefghijklmnopqrstuvwxyz "
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ "
        "0123456789";
    if (nodeCount > maxNodeCount)
        nodeCount = maxNodeCount;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> lenDist(minWordLen, maxWordLen);
    std::uniform_int_distribution<> charDist(0, chars.size() - 1);
    std::uniform_int_distribution<> randIndexDist(-1, nodeCount - 1);

    for (int i = 0; i < nodeCount; i++)
    {
        int len = lenDist(rng);

        std::string data;
        data.reserve(len);

        for (int j = 0; j < len; j++)
            data += chars[charDist(rng)];

        int randIndex = randIndexDist(rng);

        out << data << ";" << randIndex << "\n";
    }

    out.close();
}

// Чтение данных из файла
std::vector<NewListNode> readInput(const std::string& filename)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open " + filename);

    std::vector<NewListNode> nodes;
    nodes.reserve(maxNodeCount);

    std::string line;
    line.reserve(128);
    // Создание и заполнение узлов
    while (std::getline(in, line))
    {
        const char* str = line.c_str();
        const char* sep = strrchr(str, ';');

        if (!sep)
            continue;

        std::string data(str, sep - str);

        int randIndex = std::strtol(sep + 1, nullptr, 10);

        ListNode* node = new ListNode();
        node->data = std::move(data);

        nodes.push_back({node, randIndex});
    }

    size_t n = nodes.size();
    // Заполнение prev/next
    for (size_t i = 0; i < n; i++)
    {
        if (i > 0)
            nodes[i].node->prev = nodes[i - 1].node;

        if (i + 1 < n)
            nodes[i].node->next = nodes[i + 1].node;
    }
    // Заполнение rand
    for (size_t i = 0; i < n; i++)
    {
        int r = nodes[i].randIndex;
        if (r >= 0 && r < (int)n)
            nodes[i].node->rand = nodes[r].node;
    }

    return nodes;
}


// сериализация
void serialize(const std::vector<NewListNode>& nodes,
         const std::string& filename)
{
    uint32_t count = nodes.size();

    // общий размер буфера
    size_t totalSize = sizeof(count);
    for (const auto& wrap : nodes)
       totalSize += sizeof(uint32_t) + wrap.node->data.size() + sizeof(int32_t);

    // буфер
    std::vector<char> buffer(totalSize);
    char* p = buffer.data();

    // количество элементов
    memcpy(p, &count, sizeof(count));
    p += sizeof(count);

    // сериализация каждого узла
    for (const auto& wrap : nodes)
    {
       uint32_t len = wrap.node->data.size();
       int32_t randIndex = wrap.randIndex;

       memcpy(p, &len, sizeof(len));
       p += sizeof(len);

       memcpy(p, wrap.node->data.data(), len);
       p += len;

       memcpy(p, &randIndex, sizeof(randIndex));
       p += sizeof(randIndex);
    }

    // Сохранение буфера в бинарный файл
    std::ofstream out(filename, std::ios::binary);
    if (!out)
       throw std::runtime_error("cannot open " + filename);

    out.write(buffer.data(), buffer.size());
}

// Очищение памяти
void freeListMemory(const std::vector<NewListNode>& nodes)
{
    for (auto& n : nodes)
        delete n.node;
}

// Десериализация
std::vector<NewListNode> deserialize(const std::string& filename)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open " + filename);

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (count == 0) return {};

    std::vector<NewListNode> nodes;
    nodes.reserve(count);

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));

        std::string data(len, '\0');
        in.read(&data[0], len);

        int32_t randIndex = -1;
        in.read(reinterpret_cast<char*>(&randIndex), sizeof(randIndex));

        ListNode* node = new ListNode();
        node->data = std::move(data);

        nodes.push_back({node, randIndex});
    }

    for (size_t i = 0; i < nodes.size(); i++)
    {
        if (i > 0)
            nodes[i].node->prev = nodes[i - 1].node;
        if (i + 1 < nodes.size())
            nodes[i].node->next = nodes[i + 1].node;
    }

    for (size_t i = 0; i < nodes.size(); i++)
    {
        int r = nodes[i].randIndex;
        if (r >= 0 && r < (int)nodes.size())
            nodes[i].node->rand = nodes[r].node;
    }

    return nodes;
}

// Сохранение десериализованных данных в файл для сверки
void saveDeserializedListToFile(const std::vector<NewListNode>& nodes, const std::string& filename)
{
    std::ofstream out(filename);
    if (!out)
        throw std::runtime_error("cannot create file " + filename);

    for (const auto& wrap : nodes)
    {
        out << wrap.node->data << ";" << wrap.randIndex << "\n";
    }
}

using namespace std::chrono;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int reservedNodeCount  = 1000000;
    std::string inFile = "inlet.in", outFile = "outlet.out", deserializeFile = "inletDeserialize";
    try
    {
        auto t1 = steady_clock::now();
        createInputFile(inFile, reservedNodeCount);
        auto t2 = steady_clock::now();
        auto durationDeserialize = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "createInputFile time: " << durationDeserialize << " ms\n";

        t1 = steady_clock::now();
        auto nodes = readInput(inFile);
        t2 = steady_clock::now();
        durationDeserialize = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "readInput time: " << durationDeserialize << " ms\n";

        t1 = steady_clock::now();
        serialize(nodes, outFile);
        t2 = steady_clock::now();
        durationDeserialize = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "serialize time: " << durationDeserialize << " ms\n";

        freeListMemory(nodes);

        t1 = steady_clock::now();
        nodes = deserialize(outFile);
        t2 = steady_clock::now();
        durationDeserialize = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "deserialize time: " << durationDeserialize << " ms\n";

        t1 = steady_clock::now();
        saveDeserializedListToFile(nodes, deserializeFile);
        t2 = steady_clock::now();
        durationDeserialize = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "saveDeserializedListToFile time: " << durationDeserialize << " ms\n";

        freeListMemory(nodes);
        std::cout << "Done\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
    return a.exec();
}
