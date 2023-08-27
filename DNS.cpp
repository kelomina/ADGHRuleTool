#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <filesystem>

namespace fs = std::filesystem;

void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

bool isLineToBeProcessed(const std::string& line) {
    return (line.find("||") == 0) ||        // �� "||" ��ͷ
        (line.find("0.0.0.0") == 0) ||   // �� "0.0.0.0" ��ͷ
        (line.find("127.0.0.1") == 0) || // �� "127.0.0.1" ��ͷ
        (line.find("::") == 0);          // �� "::" ��ͷ
}

void processRules(const std::string& inputFilePath, const std::string& outputFilePath) {
    std::ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        std::cerr << "�޷��������ļ���" << inputFilePath << std::endl;
        return;
    }

    std::unordered_set<std::string> uniqueRules;

    std::string line;
    while (std::getline(inputFile, line)) {
        if (isLineToBeProcessed(line)) {
            size_t startPos = line.find_first_not_of(" \t");
            if (startPos != std::string::npos) {
                line.erase(std::remove(line.begin(), line.end(), '#'), line.end());
                std::string normalizedRule = line.substr(startPos);
                uniqueRules.insert(normalizedRule);
            }
        }
    }

    inputFile.close();

    std::ofstream outputFile(outputFilePath, std::ios_base::app);
    if (!outputFile.is_open()) {
        std::cerr << "�޷�������ļ���" << outputFilePath << std::endl;
        return;
    }

    for (const std::string& rule : uniqueRules) {
        outputFile << rule << std::endl;
    }

    outputFile.close();

    std::cout << "�����Ѵ���ȥ�ء�" << std::endl;
}

bool downloadRulesWithProxy(const std::string& url, const std::string& filename, CURL* curl) {
    FILE* file;
    errno_t err = fopen_s(&file, filename.c_str(), "ab");
    if (err != 0 || !file) {
        std::cerr << "�޷������ļ���" << filename << std::endl;
        return false;
    }

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_PROXY, "http://127.0.0.1:7890");
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);

        CURLcode res;
        int retryCount = 5;
        do {
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "����ʧ�ܣ�" << curl_easy_strerror(res) << std::endl;
                retryCount--;
                if (retryCount > 0) {
                    std::cout << "�ȴ�5�������..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
        } while (res != CURLE_OK && retryCount > 0);

        fclose(file);

        if (res != CURLE_OK) {
            std::cerr << "������Ժ���Ȼ�޷������ļ���" << url << std::endl;
            remove(filename.c_str());
            return false;
        }
    }
    else {
        std::cerr << "CURL ��ʼ��ʧ�ܡ�" << std::endl;
        fclose(file);
        return false;
    }

    return true;
}

void removeLinesStartingWithAsterisks(const std::string& filePath) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        std::cerr << "�޷����ļ���" << filePath << std::endl;
        return;
    }

    std::ofstream outputFile("temp_" + filePath);
    if (!outputFile.is_open()) {
        std::cerr << "�޷����ļ���" << "temp_" + filePath << std::endl;
        inputFile.close();
        return;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.find("**") != 0) {  // ���� "**" ��ͷ���б���
            outputFile << line << std::endl;
        }
    }

    inputFile.close();
    outputFile.close();

    if (remove(filePath.c_str()) == 0) {  // ɾ��ԭ�ļ�
        if (rename(("temp_" + filePath).c_str(), filePath.c_str()) != 0) {  // ��������ʱ�ļ�
            std::cerr << "�޷���������ʱ�ļ���" << ("temp_" + filePath) << std::endl;
        }
    }
    else {
        std::cerr << "�޷�ɾ���ļ���" << filePath << std::endl;
    }
}

int main() {
    std::string outputFilePath = fs::absolute("output_rules.txt").string();
    CURL* curl = curl_easy_init();

    std::vector<std::string> ruleUrls;

    std::ifstream ruleListFile(fs::absolute("rule.txt").string());
    if (!ruleListFile.is_open()) {
        std::cerr << "�޷��򿪹����б��ļ���" << fs::absolute("rule.txt").string() << std::endl;
        return 1;
    }

    std::string ruleUrl;
    while (std::getline(ruleListFile, ruleUrl)) {
        ruleUrls.push_back(ruleUrl);
    }

    curl_easy_cleanup(curl);

    std::cout << "���� Enter ����ʼ�Զ�ѭ��ִ�У�ÿ6Сʱִ��һ�Ρ�" << std::endl;
    std::cin.get();
    clearConsole();  // ����ն�����
    while (true) {
        outputFilePath = fs::absolute("output_rules.txt").string();
        std::remove("temp_rules.txt");
        std::remove(outputFilePath.c_str());
        curl = curl_easy_init();

        std::int64_t nextExecutionTime = std::chrono::system_clock::now().time_since_epoch().count() + 6 * 60 * 60;

        for (size_t i = 0; i < ruleUrls.size(); ++i) {
            std::string tempFilename = "temp_rules_" + std::to_string(i) + ".txt";

            if (downloadRulesWithProxy(ruleUrls[i], tempFilename, curl)) {
                processRules(tempFilename, outputFilePath);
                remove(tempFilename.c_str());
            }
        }

        curl_easy_cleanup(curl);

        clearConsole();

        std::cout << "�����ļ������ء��ϲ���ȥ�ء���һ��ִ�н���6Сʱ��";
        std::cout << "���С�" << std::endl;

        std::this_thread::sleep_until(std::chrono::system_clock::time_point(std::chrono::seconds(nextExecutionTime)));

        removeLinesStartingWithAsterisks(outputFilePath);
    }

    return 0;
}