#include "../json/src/json.hpp"

#include <fstream>
#include <string>

using json = nlohmann::json;

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    std::cout << "need parameter" << std::endl;
    return -1;
  }

  json cfg_json;
  try {
    cfg_json = json::parse(std::ifstream(argv[1]));
  } catch (std::invalid_argument& inv_arg) {
    std::cout << "invalid argument" << std::endl;
    return -2;
  }
  std::cout << static_cast<int>(cfg_json["coordinator"]["tcp_port"]) << std::endl;
  return 0;
}
