#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

double convertISOStringToTimestamp(const std::string& timestampStr) {
    std::tm t = {};
    std::istringstream ss(timestampStr);
    ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%SZ");  // 根据时间戳的格式读取字符串

    std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t(std::mktime(&t));
    std::chrono::duration<double> duration = tp.time_since_epoch();

    return duration.count();  // 将时间点转换为 double 类型的时间戳
}

int main() {
    std::string timestampStr = "2024-01-15T08:22:11Z";
    double timestamp = convertISOStringToTimestamp(timestampStr);
    std::cout << "Timestamp as double: " << timestamp << std::endl;
    return 0;
}
