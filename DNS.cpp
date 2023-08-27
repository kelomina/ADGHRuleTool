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

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

bool isLineToBeProcessed(const std::string& line) {
    return (line.find("||") == 0) ||        // 以 "||" 开头
        (line.find("0.0.0.0") == 0) ||   // 以 "0.0.0.0" 开头
        (line.find("127.0.0.1") == 0) || // 以 "127.0.0.1" 开头
        (line.find("::") == 0);          // 以 "::" 开头
}

void processRules(const std::string& inputFilePath, const std::string& outputFilePath) {
    std::ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        std::cerr << "无法打开输入文件：" << inputFilePath << std::endl;
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
        std::cerr << "无法打开输出文件：" << outputFilePath << std::endl;
        return;
    }

    for (const std::string& rule : uniqueRules) {
        outputFile << rule << std::endl;
    }

    outputFile.close();

    std::cout << "规则已处理并去重。" << std::endl;
}

bool downloadRulesWithProxy(const std::string& url, const std::string& filename, CURL* curl) {
    FILE* file;
#ifdef _WIN32
    errno_t err = fopen_s(&file, filename.c_str(), "ab");
#else
    file = fopen(filename.c_str(), "ab");
#endif
    if (!file) {
        std::cerr << "无法创建文件：" << filename << std::endl;
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
                std::cerr << "下载失败：" << curl_easy_strerror(res) << std::endl;
                retryCount--;
                if (retryCount > 0) {
                    std::cout << "等待5秒后重试..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
        } while (res != CURLE_OK && retryCount > 0);

        fclose(file);

        if (res != CURLE_OK) {
            std::cerr << "多次重试后仍然无法下载文件：" << url << std::endl;
            remove(filename.c_str());
            return false;
        }
    }
    else {
        std::cerr << "CURL 初始化失败。" << std::endl;
        fclose(file);
        return false;
    }

    return true;
}

void removeLinesStartingWithAsterisks(const std::string& filePath) {
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        std::cerr << "无法打开文件：" << filePath << std::endl;
        return;
    }

    std::ofstream outputFile("temp_" + filePath);
    if (!outputFile.is_open()) {
        std::cerr << "无法打开文件：" << "temp_" + filePath << std::endl;
        inputFile.close();
        return;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.find("**") != 0) {  // 不以 "**" 开头的行保留
            outputFile << line << std::endl;
        }
    }

    inputFile.close();
    outputFile.close();

    if (remove(filePath.c_str()) == 0) {  // 删除原文件
        if (rename(("temp_" + filePath).c_str(), filePath.c_str()) != 0) {  // 重命名临时文件
            std::cerr << "无法重命名临时文件：" << ("temp_" + filePath) << std::endl;
        }
    }
    else {
        std::cerr << "无法删除文件：" << filePath << std::endl;
    }
}

int main() {
    std::string outputFilePath = fs::absolute("output_rules.txt").string();
    CURL* curl = curl_easy_init();

    std::vector<std::string> ruleUrls;

    std::ifstream ruleListFile(fs::absolute("rule.txt").string());
    if (!ruleListFile.is_open()) {
        std::cerr << "无法打开规则列表文件：" << fs::absolute("rule.txt").string() << std::endl;
        return 1;
    }

    std::string ruleUrl;
    while (std::getline(ruleListFile, ruleUrl)) {
        ruleUrls.push_back(ruleUrl);
    }

    curl_easy_cleanup(curl);

    std::cout << "按下 Enter 键开始自动循环执行，每6小时执行一次。" << std::endl;
    std::cin.get();
    clearConsole();  // 清空终端内容
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

        std::cout << "规则文件已下载、合并并去重。下一次执行将在6小时后进行。" << std::endl;

#ifdef _WIN32
        Sleep(6 * 60 * 60 * 1000);  // Windows下的睡眠函数
#else
        sleep(6 * 60 * 60);  // Linux下的睡眠函数
#endif

        removeLinesStartingWithAsterisks(outputFilePath);
    }

    return 0;
}
